#pragma once

#include "atlasdb/execution/expressions/expression.hpp"

#include <memory>

namespace atlasdb {

enum class ComparisonOperator {
    Equal,
    NotEqual,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual
};

class ComparisonExpression final : public Expression {
public:
    ComparisonExpression(
        ComparisonOperator op,
        std::unique_ptr<Expression> left,
        std::unique_ptr<Expression> right
    );

    [[nodiscard]]
    auto evaluate(const std::vector<Value>& row) const -> Value override;

private:
    ComparisonOperator op_;
    std::unique_ptr<Expression> left_;
    std::unique_ptr<Expression> right_;
};

}
