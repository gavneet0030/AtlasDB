#include "atlasdb/execution/expressions/literal_expression.hpp"

#include <utility>

namespace atlasdb {

LiteralExpression::LiteralExpression(Value value)
    : value_(std::move(value)) {}

auto LiteralExpression::evaluate(const std::vector<Value>& row) const -> Value {
    (void)row;
    return value_;
}

}  // namespace atlasdb
