#pragma once

#include "atlasdb/catalog/schema.hpp"
#include "atlasdb/types/value.hpp"

#include <vector>

namespace atlasdb {

class ExecutionOperator {
public:
    virtual ~ExecutionOperator() = default;

    [[nodiscard]] virtual auto schema() const noexcept
        -> const Schema& = 0;

    virtual auto next(std::vector<Value>& output_row) -> bool = 0;
};

}  // namespace atlasdb
