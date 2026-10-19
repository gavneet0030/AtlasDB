#pragma once

#include "atlasdb/execution/expressions/expression.hpp"

#include <memory>

namespace atlasdb {

enum class LogicalOperator {
    And,
    Or,
    Not
};

class LogicalExpression final : public Expression {
public:
    // Binary operators: AND and OR
    LogicalExpression(
        LogicalOperator op,
        std::unique_ptr<Expression> left,
        std::unique_ptr<Expression> right);

    // Unary operator: NOT
    LogicalExpression(
        LogicalOperator op,
        std::unique_ptr<Expression> operand);

    [[nodiscard]] auto evaluate(
        const std::vector<Value>& row) const -> Value override;

private:
    LogicalOperator op_;
    std::unique_ptr<Expression> left_;
    std::unique_ptr<Expression> right_;
};

}  // namespace atlasdb
