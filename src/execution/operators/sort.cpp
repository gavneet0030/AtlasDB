#include "atlasdb/execution/operators/sort.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace atlasdb {
namespace {

auto compare_double(double lhs, double rhs) -> int {
    const bool lhs_nan = std::isnan(lhs);
    const bool rhs_nan = std::isnan(rhs);

    if (lhs_nan && rhs_nan) {
        return 0;
    }
    if (lhs_nan) {
        return 1;
    }
    if (rhs_nan) {
        return -1;
    }
    if (lhs < rhs) {
        return -1;
    }
    if (lhs > rhs) {
        return 1;
    }
    return 0;
}

auto compare_values(
    const Value& lhs,
    const Value& rhs,
    DataType type
) -> int {
    switch (type) {
    case DataType::Int64: {
        const auto& left = lhs.as<std::int64_t>();
        const auto& right = rhs.as<std::int64_t>();

        if (left < right) return -1;
        if (left > right) return 1;
        return 0;
    }
    case DataType::Double:
        return compare_double(lhs.as<double>(), rhs.as<double>());

    case DataType::Boolean: {
        const auto left = lhs.as<bool>();
        const auto right = rhs.as<bool>();

        if (left == right) return 0;
        return left ? 1 : -1;
    }
    case DataType::String: {
        const auto& left = lhs.as<std::string>();
        const auto& right = rhs.as<std::string>();

        if (left < right) return -1;
        if (left > right) return 1;
        return 0;
    }
    }

    throw std::invalid_argument("Unsupported data type in Sort.");
}

} // namespace

Sort::Sort(
    std::unique_ptr<ExecutionOperator> input,
    std::vector<SortKey> keys
)
    : input_(std::move(input)),
      keys_(std::move(keys)),
      output_schema_(
          input_ ? input_->schema() : Schema(std::vector<Column>{})
      ) {
    if (!input_) {
        throw std::invalid_argument("Sort requires a non-null input.");
    }

    if (keys_.empty()) {
        throw std::invalid_argument("Sort requires at least one sort key.");
    }

    for (const auto& key : keys_) {
        if (key.column_index >= output_schema_.column_count()) {
            throw std::invalid_argument(
                "Sort key column index is out of range."
            );
        }
    }
}

auto Sort::schema() const noexcept -> const Schema& {
    return output_schema_;
}

void Sort::load_and_sort() {
    if (loaded_) {
        return;
    }

    std::vector<Value> row;

    while (input_->next(row)) {
        if (row.size() != output_schema_.column_count()) {
            throw std::invalid_argument(
                "Input row does not match the Sort schema."
            );
        }

        rows_.push_back(row);
    }

    std::stable_sort(
        rows_.begin(),
        rows_.end(),
        [this](const auto& lhs, const auto& rhs) {
            for (const auto& key : keys_) {
                const auto& left = lhs[key.column_index];
                const auto& right = rhs[key.column_index];

                if (left.is_null() && right.is_null()) {
                    continue;
                }

                if (left.is_null()) {
                    return key.nulls == NullPlacement::First;
                }

                if (right.is_null()) {
                    return key.nulls == NullPlacement::Last;
                }

                const auto type =
                    output_schema_.column_at(key.column_index).type;

                const int comparison = compare_values(left, right, type);

                if (comparison == 0) {
                    continue;
                }

                return key.direction == SortDirection::Ascending
                    ? comparison < 0
                    : comparison > 0;
            }

            return false;
        }
    );

    loaded_ = true;
}

auto Sort::next(std::vector<Value>& output_row) -> bool {
    load_and_sort();

    if (next_index_ >= rows_.size()) {
        output_row.clear();
        return false;
    }

    output_row = rows_[next_index_];
    ++next_index_;
    return true;
}

} // namespace atlasdb
