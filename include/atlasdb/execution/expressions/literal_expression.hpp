#pragma once

#include "atlasdb/execution/expressions/expression.hpp"

namespace atlasdb {

class LiteralExpression final : public Expression {
public:
    explicit LiteralExpression(Value value);

    [[nodiscard]]
    auto evaluate(const std::vector<Value>& row) const -> Value override;

private:
    Value value_;
};

}  // namespace atlasdb
