#pragma once

#include <cstddef>
#include <vector>

#include "atlasdb/types/data_type.hpp"
#include "atlasdb/types/value.hpp"

namespace atlasdb {

class ColumnVector {
public:
    explicit ColumnVector(DataType type);

    [[nodiscard]] auto type() const noexcept -> DataType;
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;

    void reserve(std::size_t capacity);
    void append(const Value& value);
    void append(Value&& value);
    void clear() noexcept;

    [[nodiscard]] auto at(std::size_t index) const -> const Value&;
    [[nodiscard]] auto values() const noexcept -> const std::vector<Value>&;

private:
    DataType type_;
    std::vector<Value> values_;
};

}  // namespace atlasdb