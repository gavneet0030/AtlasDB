#include "atlasdb/execution/expressions/logical_expression.hpp"

#include <stdexcept>
#include <utility>

namespace atlasdb {

LogicalExpression::LogicalExpression(
    LogicalOperator op,
    std::unique_ptr<Expression> left,
    std::unique_ptr<Expression> right)
    : op_(op), left_(std::move(left)), right_(std::move(right)) {
    if (op_ == LogicalOperator::Not) {
        throw std::invalid_argument("NOT requires exactly one operand");
    }

    if (!left_ || !right_) {
        throw std::invalid_argument("AND and OR require two operands");
    }
}

LogicalExpression::LogicalExpression(
    LogicalOperator op,
    std::unique_ptr<Expression> operand)
    : op_(op), left_(std::move(operand)), right_(nullptr) {
    if (op_ != LogicalOperator::Not) {
        throw std::invalid_argument(
            "The unary constructor is only valid for NOT");
    }

    if (!left_) {
        throw std::invalid_argument("NOT requires one operand");
    }
}

auto LogicalExpression::evaluate(
    const std::vector<Value>& row) const -> Value {
    const Value left_value = left_->evaluate(row);

    if (op_ == LogicalOperator::Not) {
        if (left_value.is_null()) {
            return Value::null();
        }

        if (!left_value.is_compatible_with(DataType::Boolean)) {
            throw std::invalid_argument("NOT requires a Boolean operand");
        }

        return Value(!left_value.as<bool>());
    }

    // SQL three-valued logic allows NULL but not non-Boolean values.
    if (!left_value.is_null() &&
        !left_value.is_compatible_with(DataType::Boolean)) {
        throw std::invalid_argument(
            "Logical operators require Boolean operands");
    }

    // FALSE AND anything = FALSE.
    if (op_ == LogicalOperator::And &&
        !left_value.is_null() && !left_value.as<bool>()) {
        return Value(false);
    }

    // TRUE OR anything = TRUE.
    if (op_ == LogicalOperator::Or &&
        !left_value.is_null() && left_value.as<bool>()) {
        return Value(true);
    }

    const Value right_value = right_->evaluate(row);

    if (!right_value.is_null() &&
        !right_value.is_compatible_with(DataType::Boolean)) {
        throw std::invalid_argument(
            "Logical operators require Boolean operands");
    }

    if (op_ == LogicalOperator::And) {
        // At this point, the left side is TRUE or NULL.
        if (!right_value.is_null() && !right_value.as<bool>()) {
            return Value(false);
        }

        if (left_value.is_null() || right_value.is_null()) {
            return Value::null();
        }

        return Value(true);
    }

    // OR: at this point, the left side is FALSE or NULL.
    if (!right_value.is_null() && right_value.as<bool>()) {
        return Value(true);
    }

    if (left_value.is_null() || right_value.is_null()) {
        return Value::null();
    }

    return Value(false);
}

}  // namespace atlasdb
