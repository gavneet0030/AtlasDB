#include "atlasdb/execution/operators/aggregation.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace atlasdb {
namespace {

auto aggregate_name(
    const Schema& schema,
    const AggregateSpec& aggregate) -> std::string
{
    if (aggregate.function == AggregateFunction::Count &&
        !aggregate.column_index.has_value()) {
        return "COUNT(*)";
    }

    if (!aggregate.column_index.has_value()) {
        throw std::invalid_argument(
            "Aggregate requires a column index");
    }

    const auto& column =
        schema.column_at(aggregate.column_index.value());

    std::string function_name;
    switch (aggregate.function) {
    case AggregateFunction::Count: function_name = "COUNT"; break;
    case AggregateFunction::Sum: function_name = "SUM"; break;
    case AggregateFunction::Min: function_name = "MIN"; break;
    case AggregateFunction::Max: function_name = "MAX"; break;
    case AggregateFunction::Avg: function_name = "AVG"; break;
    }

    return function_name + "(" + column.name + ")";
}

auto aggregate_type(
    const Schema& schema,
    const AggregateSpec& aggregate) -> DataType
{
    if (aggregate.function == AggregateFunction::Count) {
        return DataType::Int64;
    }

    if (!aggregate.column_index.has_value()) {
        throw std::invalid_argument(
            "Only COUNT supports a missing column index");
    }

    const auto type =
        schema.column_at(aggregate.column_index.value()).type;

    if (type != DataType::Int64 && type != DataType::Double) {
        throw std::invalid_argument(
            "Numeric aggregates require an Int64 or Double column");
    }

    return aggregate.function == AggregateFunction::Avg
        ? DataType::Double
        : type;
}

auto make_output_schema(
    const Schema& input_schema,
    const std::vector<std::size_t>& group_columns,
    const std::vector<AggregateSpec>& aggregates) -> Schema
{
    if (aggregates.empty()) {
        throw std::invalid_argument(
            "Aggregation requires at least one aggregate");
    }

    std::vector<Column> columns;
    columns.reserve(group_columns.size() + aggregates.size());

    for (const auto index : group_columns) {
        columns.push_back(input_schema.column_at(index));
    }

    for (const auto& aggregate : aggregates) {
        columns.push_back({
            aggregate_name(input_schema, aggregate),
            aggregate_type(input_schema, aggregate)});
    }

    return Schema(std::move(columns));
}

struct AggregateState {
    std::int64_t count{0};
    std::int64_t integer_sum{0};
    double double_sum{0.0};
    bool has_value{false};
    Value extremum;
};

struct GroupState {
    std::vector<Value> key;
    std::vector<AggregateState> aggregates;
};

auto numeric_value(const Value& value) -> double
{
    if (value.data_type() == DataType::Int64) {
        return static_cast<double>(value.as<std::int64_t>());
    }
    return value.as<double>();
}

void checked_add(std::int64_t& total, std::int64_t value)
{
    if ((value > 0 &&
         total > std::numeric_limits<std::int64_t>::max() - value) ||
        (value < 0 &&
         total < std::numeric_limits<std::int64_t>::min() - value)) {
        throw std::overflow_error("Integer SUM overflow");
    }

    total += value;
}

auto same_key(
    const std::vector<Value>& left,
    const std::vector<Value>& right) -> bool
{
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].storage() != right[i].storage()) {
            return false;
        }
    }

    return true;
}

void accumulate(
    const std::vector<Value>& row,
    const std::vector<AggregateSpec>& specs,
    std::vector<AggregateState>& states)
{
    for (std::size_t i = 0; i < specs.size(); ++i) {
        const auto& spec = specs[i];
        auto& state = states[i];

        if (spec.function == AggregateFunction::Count &&
            !spec.column_index.has_value()) {
            ++state.count;
            continue;
        }

        const auto& value = row.at(spec.column_index.value());
        if (value.is_null()) {
            continue;
        }

        if (spec.function == AggregateFunction::Count) {
            ++state.count;
            continue;
        }

        switch (spec.function) {
        case AggregateFunction::Sum:
            if (value.data_type() == DataType::Int64) {
                checked_add(state.integer_sum, value.as<std::int64_t>());
            } else {
                state.double_sum += value.as<double>();
            }
            state.has_value = true;
            break;

        case AggregateFunction::Avg:
            state.double_sum += numeric_value(value);
            ++state.count;
            state.has_value = true;
            break;

        case AggregateFunction::Min:
        case AggregateFunction::Max:
            if (!state.has_value) {
                state.extremum = value;
                state.has_value = true;
            } else {
                bool replace = false;
                if (value.data_type() == DataType::Int64) {
                    const auto current = value.as<std::int64_t>();
                    const auto previous =
                        state.extremum.as<std::int64_t>();
                    replace =
                        spec.function == AggregateFunction::Min
                            ? current < previous
                            : current > previous;
                } else {
                    const double current = value.as<double>();
                    const double previous =
                        state.extremum.as<double>();
                    replace =
                        spec.function == AggregateFunction::Min
                            ? current < previous
                            : current > previous;
                }
                if (replace) {
                    state.extremum = value;
                }
            }
            break;

        case AggregateFunction::Count:
            break;
        }
    }
}

auto finalize(
    const Schema& input_schema,
    const std::vector<AggregateSpec>& specs,
    const std::vector<AggregateState>& states) -> std::vector<Value>
{
    std::vector<Value> row;
    row.reserve(specs.size());

    for (std::size_t i = 0; i < specs.size(); ++i) {
        const auto& spec = specs[i];
        const auto& state = states[i];

        switch (spec.function) {
        case AggregateFunction::Count:
            row.emplace_back(state.count);
            break;

        case AggregateFunction::Sum:
            if (!state.has_value) {
                row.push_back(Value::null());
            } else if (input_schema.column_at(
                           spec.column_index.value()).type ==
                       DataType::Int64) {
                row.emplace_back(state.integer_sum);
            } else {
                row.emplace_back(state.double_sum);
            }
            break;

        case AggregateFunction::Avg:
            if (!state.has_value) {
                row.push_back(Value::null());
            } else {
                row.emplace_back(
                    state.double_sum / static_cast<double>(state.count));
            }
            break;

        case AggregateFunction::Min:
        case AggregateFunction::Max:
            row.push_back(
                state.has_value ? state.extremum : Value::null());
            break;
        }
    }

    return row;
}

}  // namespace

Aggregation::Aggregation(
    std::unique_ptr<ExecutionOperator> input,
    std::vector<AggregateSpec> aggregates)
    : Aggregation(
          std::move(input),
          std::vector<std::size_t>{},
          std::move(aggregates))
{
}

Aggregation::Aggregation(
    std::unique_ptr<ExecutionOperator> input,
    std::vector<std::size_t> group_by_columns,
    std::vector<AggregateSpec> aggregates)
    : input_(std::move(input)),
      group_by_columns_(std::move(group_by_columns)),
      aggregates_(std::move(aggregates)),
      output_schema_(
          input_ ? make_output_schema(
                       input_->schema(), group_by_columns_, aggregates_)
                 : Schema(std::vector<Column>{}))
{
    if (!input_) {
        throw std::invalid_argument(
            "Aggregation input must not be null");
    }
}

auto Aggregation::schema() const noexcept -> const Schema&
{
    return output_schema_;
}

auto Aggregation::next(std::vector<Value>& output_row) -> bool
{
    if (!materialized_) {
        materialized_ = true;
        std::vector<GroupState> groups;
        std::vector<Value> input_row;

        while (input_->next(input_row)) {
            std::vector<Value> key;
            key.reserve(group_by_columns_.size());
            for (const auto index : group_by_columns_) {
                key.push_back(input_row.at(index));
            }

            auto it = std::find_if(
                groups.begin(), groups.end(),
                [&key](const GroupState& group) {
                    return same_key(group.key, key);
                });

            if (it == groups.end()) {
                groups.push_back({
                    key,
                    std::vector<AggregateState>(aggregates_.size())});
                it = std::prev(groups.end());
            }

            accumulate(input_row, aggregates_, it->aggregates);
        }

        // A global aggregate returns one row even when input is empty.
        // GROUP BY over empty input returns zero rows.
        if (group_by_columns_.empty() && groups.empty()) {
            groups.push_back({
                {},
                std::vector<AggregateState>(aggregates_.size())});
        }

        result_rows_.reserve(groups.size());
        for (const auto& group : groups) {
            auto result = group.key;
            auto aggregate_values = finalize(
                input_->schema(), aggregates_, group.aggregates);
            result.insert(
                result.end(),
                aggregate_values.begin(),
                aggregate_values.end());
            result_rows_.push_back(std::move(result));
        }
    }

    if (next_result_ >= result_rows_.size()) {
        output_row.clear();
        return false;
    }

    output_row = result_rows_[next_result_++];
    return true;
}

}  // namespace atlasdb