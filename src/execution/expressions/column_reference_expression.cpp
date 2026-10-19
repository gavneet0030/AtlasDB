#include "atlasdb/execution/expressions/column_reference_expression.hpp"

namespace atlasdb {

ColumnReferenceExpression::ColumnReferenceExpression(
    std::size_t column_index) noexcept
    : column_index_(column_index) {}

auto ColumnReferenceExpression::evaluate(
    const std::vector<Value>& row) const -> Value {
    return row.at(column_index_);
}

}  // namespace atlasdb
