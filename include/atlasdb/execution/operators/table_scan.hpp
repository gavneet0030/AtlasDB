#pragma once

#include "atlasdb/execution/operators/execution_operator.hpp"
#include "atlasdb/storage/table.hpp"

#include <cstddef>

namespace atlasdb {

class TableScan final : public ExecutionOperator {
public:
    explicit TableScan(const Table& table) noexcept;

    [[nodiscard]] auto schema() const noexcept
        -> const Schema& override;

    auto next(std::vector<Value>& output_row) -> bool override;

private:
    const Table& table_;
    std::size_t next_row_{0};
};

}  // namespace atlasdb
