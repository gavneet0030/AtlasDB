#pragma once

#include "atlasdb/execution/expressions/expression.hpp"

#include <cstddef>

namespace atlasdb {

class ColumnReferenceExpression final : public Expression {
public:
    explicit ColumnReferenceExpression(std::size_t column_index) noexcept;

    [[nodiscard]]
    auto evaluate(const std::vector<Value>& row) const -> Value override;

private:
    std::size_t column_index_;
};

}  // namespace atlasdb
