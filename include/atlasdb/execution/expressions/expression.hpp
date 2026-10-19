#pragma once

#include "atlasdb/types/value.hpp"

#include <vector>

namespace atlasdb {

class Expression {
public:
    virtual ~Expression() = default;

    [[nodiscard]]
    virtual auto evaluate(const std::vector<Value>& row) const -> Value = 0;
};

}  // namespace atlasdb
