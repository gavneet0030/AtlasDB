#include "atlasdb/execution/expressions/comparison_expression.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace atlasdb {
namespace {

auto is_numeric(DataType type) -> bool {
    return type == DataType::Int64 || type == DataType::Double;
}

auto numeric_value(const Value& value) -> long double {
    if (value.data_type() == DataType::Int64) {
        return static_cast<long double>(value.as<std::int64_t>());
    }

    if (value.data_type() == DataType::Double) {
        return static_cast<long double>(value.as<double>());
    }

    throw std::invalid_argument("Value is not numeric");
}

auto apply_comparison(
    int result,
    ComparisonOperator op
) -> bool {
    switch (op) {
        case ComparisonOperator::Equal:
            return result == 0;
        case ComparisonOperator::NotEqual:
            return result != 0;
        case ComparisonOperator::LessThan:
            return result < 0;
        case ComparisonOperator::LessThanOrEqual:
            return result <= 0;
        case ComparisonOperator::GreaterThan:
            return result > 0;
        case ComparisonOperator::GreaterThanOrEqual:
            return result >= 0;
    }

    throw std::invalid_argument("Unknown comparison operator");
}

auto compare_numeric(
    const Value& left,
    const Value& right,
    ComparisonOperator op
) -> Value {
    const auto lhs = numeric_value(left);
    const auto rhs = numeric_value(right);

    if (std::isnan(lhs) || std::isnan(rhs)) {
        return Value(op == ComparisonOperator::NotEqual);
    }

    const int result = (lhs > rhs) - (lhs < rhs);
    return Value(apply_comparison(result, op));
}

}  // namespace

ComparisonExpression::ComparisonExpression(
    ComparisonOperator op,
    std::unique_ptr<Expression> left,
    std::unique_ptr<Expression> right
)
    : op_(op),
      left_(std::move(left)),
      right_(std::move(right)) {
    if (!left_ || !right_) {
        throw std::invalid_argument(
            "Comparison operands must not be null"
        );
    }
}

auto ComparisonExpression::evaluate(
    const std::vector<Value>& row
) const -> Value {
    const Value left = left_->evaluate(row);
    const Value right = right_->evaluate(row);

    // SQL comparisons involving NULL produce UNKNOWN.
    if (left.is_null() || right.is_null()) {
        return Value::null();
    }

    const auto left_type = left.data_type();
    const auto right_type = right.data_type();

    if (!left_type || !right_type) {
        throw std::invalid_argument(
            "Cannot compare values with unknown types"
        );
    }

    // Integers and doubles can be compared numerically.
    if (is_numeric(*left_type) && is_numeric(*right_type)) {
        return compare_numeric(left, right, op_);
    }

    // Values of different non-numeric types are incompatible.
    if (*left_type != *right_type) {
        throw std::invalid_argument(
            "Cannot compare incompatible value types"
        );
    }

    switch (*left_type) {
        case DataType::String: {
            const auto& lhs = left.as<std::string>();
            const auto& rhs = right.as<std::string>();
            const int result = (lhs > rhs) - (lhs < rhs);
            return Value(apply_comparison(result, op_));
        }

        case DataType::Boolean: {
            if (op_ != ComparisonOperator::Equal &&
                op_ != ComparisonOperator::NotEqual) {
                throw std::invalid_argument(
                    "Ordering comparisons are not supported for booleans"
                );
            }

            const bool equal =
                left.as<bool>() == right.as<bool>();

            return Value(
                op_ == ComparisonOperator::Equal ? equal : !equal
            );
        }

        case DataType::Int64:
        case DataType::Double:
            break;
    }

    throw std::invalid_argument("Unsupported comparison");
}

}
