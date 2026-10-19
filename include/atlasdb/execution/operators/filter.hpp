#pragma once

#include "atlasdb/execution/operators/execution_operator.hpp"
#include "atlasdb/execution/expressions/expression.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace atlasdb {

class Filter final : public ExecutionOperator {
public:
    using Predicate = std::function<bool(const std::vector<Value>&)>;

    Filter(std::unique_ptr<ExecutionOperator> input, Predicate predicate);

    [[nodiscard]] static auto with_expression(
        std::unique_ptr<ExecutionOperator> input,
        std::unique_ptr<Expression> predicate) -> Filter;

    [[nodiscard]] auto schema() const noexcept
        -> const Schema& override;

    auto next(std::vector<Value>& output_row) -> bool override;

private:
    struct ExpressionTag {};

    Filter(
        std::unique_ptr<ExecutionOperator> input,
        std::unique_ptr<Expression> predicate,
        ExpressionTag);

    std::unique_ptr<ExecutionOperator> input_;
    Predicate predicate_;
    std::unique_ptr<Expression> expression_;
};

}  // namespace atlasdb
