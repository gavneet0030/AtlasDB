#pragma once

#include "atlasdb/execution/operators/execution_operator.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace atlasdb {

class Projection final : public ExecutionOperator {
public:
    Projection(
        std::unique_ptr<ExecutionOperator> input,
        std::vector<std::size_t> column_indices);

    [[nodiscard]] auto schema() const noexcept
        -> const Schema& override;

    auto next(std::vector<Value>& output_row) -> bool override;

private:
    std::unique_ptr<ExecutionOperator> input_;
    std::vector<std::size_t> column_indices_;
    Schema output_schema_;
};

}  // namespace atlasdb
