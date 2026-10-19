#pragma once

#include <cstddef>
#include <vector>

#include "atlasdb/catalog/schema.hpp"
#include "atlasdb/storage/column_vector.hpp"
#include "atlasdb/types/value.hpp"

namespace atlasdb {

class RecordBatch {
public:
    explicit RecordBatch(Schema schema);

    [[nodiscard]] auto schema() const noexcept -> const Schema&;
    [[nodiscard]] auto column_count() const noexcept -> std::size_t;
    [[nodiscard]] auto row_count() const noexcept -> std::size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;

    void reserve_rows(std::size_t capacity);
    void append_row(const std::vector<Value>& row);
    void append_row(std::vector<Value>&& row);
    void clear() noexcept;

    [[nodiscard]] auto column(std::size_t index) const
        -> const ColumnVector&;

    [[nodiscard]] auto column(std::size_t index)
        -> ColumnVector&;

    [[nodiscard]] auto value_at(
        std::size_t row_index,
        std::size_t column_index) const -> const Value&;

private:
    void validate_row(const std::vector<Value>& row) const;

    Schema schema_;
    std::vector<ColumnVector> columns_;
    std::size_t row_count_{0};
};

}  // namespace atlasdb