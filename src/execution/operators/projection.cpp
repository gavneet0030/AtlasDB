#include "atlasdb/execution/operators/projection.hpp"

#include <stdexcept>
#include <utility>

namespace atlasdb {
namespace {

auto make_projection_schema(
    const Schema& input_schema,
    const std::vector<std::size_t>& column_indices) -> Schema
{
    std::vector<Column> columns;
    columns.reserve(column_indices.size());

    for (const auto index : column_indices) {
        const auto& column = input_schema.column_at(index);
        columns.push_back(column);
    }

    return Schema(std::move(columns));
}

}  // namespace

Projection::Projection(
    std::unique_ptr<ExecutionOperator> input,
    std::vector<std::size_t> column_indices)
    : input_(std::move(input)),
      column_indices_(std::move(column_indices)),
      output_schema_(
          input_ ? make_projection_schema(
                       input_->schema(), column_indices_)
                 : Schema(std::vector<Column>{}))
{
    if (!input_) {
        throw std::invalid_argument(
            "Projection input must not be null");
    }
}

auto Projection::schema() const noexcept -> const Schema&
{
    return output_schema_;
}

auto Projection::next(std::vector<Value>& output_row) -> bool
{
    std::vector<Value> input_row;

    if (!input_->next(input_row)) {
        output_row.clear();
        return false;
    }

    output_row.clear();
    output_row.reserve(column_indices_.size());

    for (const auto index : column_indices_) {
        output_row.push_back(input_row.at(index));
    }

    return true;
}

}  // namespace atlasdb
