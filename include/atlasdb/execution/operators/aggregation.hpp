#pragma once

#include "atlasdb/execution/operators/execution_operator.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace atlasdb {

enum class AggregateFunction {
    Count,
    Sum,
    Min,
    Max,
    Avg
};

struct AggregateSpec {
    AggregateFunction function;
    // nullopt represents COUNT(*); other functions require a column.
    std::optional<std::size_t> column_index;
};

class Aggregation final : public ExecutionOperator {
public:
    Aggregation(
        std::unique_ptr<ExecutionOperator> input,
        std::vector<AggregateSpec> aggregates);

    Aggregation(
        std::unique_ptr<ExecutionOperator> input,
        std::vector<std::size_t> group_by_columns,
        std::vector<AggregateSpec> aggregates);

    [[nodiscard]] auto schema() const noexcept
        -> const Schema& override;

    auto next(std::vector<Value>& output_row) -> bool override;

private:
    std::unique_ptr<ExecutionOperator> input_;
    std::vector<std::size_t> group_by_columns_;
    std::vector<AggregateSpec> aggregates_;
    Schema output_schema_;
    std::vector<std::vector<Value>> result_rows_;
    std::size_t next_result_{0};
    bool materialized_{false};
};

}  // namespace atlasdb