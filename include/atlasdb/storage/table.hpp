#pragma once

#include <cstddef>
#include <vector>

#include "atlasdb/catalog/schema.hpp"
#include "atlasdb/storage/record_batch.hpp"
#include "atlasdb/types/value.hpp"

namespace atlasdb {

class Table {
public:
    explicit Table(Schema schema);

    [[nodiscard]] auto schema() const noexcept -> const Schema&;
    [[nodiscard]] auto row_count() const noexcept -> std::size_t;
    [[nodiscard]] auto batch_count() const noexcept -> std::size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;

    void append_row(const std::vector<Value>& row);
    void append_row(std::vector<Value>&& row);
    void append_batch(RecordBatch batch);

    [[nodiscard]] auto value_at(
        std::size_t row_index,
        std::size_t column_index) const -> const Value&;

    [[nodiscard]] auto batch(std::size_t index) const
        -> const RecordBatch&;

    void clear() noexcept;

private:
    [[nodiscard]] auto schemas_match(const Schema& other) const noexcept
        -> bool;

    Schema schema_;
    std::vector<RecordBatch> batches_;
    std::size_t row_count_{0};
};

}  // namespace atlasdb