#pragma once

#include "atlasdb/execution/operators/execution_operator.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace atlasdb {

enum class SortDirection {
    Ascending,
    Descending
};

enum class NullPlacement {
    First,
    Last
};

struct SortKey {
    std::size_t column_index;
    SortDirection direction{SortDirection::Ascending};
    NullPlacement nulls{NullPlacement::Last};
};

class Sort final : public ExecutionOperator {
public:
    Sort(
        std::unique_ptr<ExecutionOperator> input,
        std::vector<SortKey> keys
    );

    [[nodiscard]] auto schema() const noexcept -> const Schema& override;

    auto next(std::vector<Value>& output_row) -> bool override;

private:
    void load_and_sort();

    std::unique_ptr<ExecutionOperator> input_;
    std::vector<SortKey> keys_;
    Schema output_schema_;
    std::vector<std::vector<Value>> rows_;
    std::size_t next_index_{0};
    bool loaded_{false};
};

} // namespace atlasdb
