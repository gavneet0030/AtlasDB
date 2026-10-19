#include "atlasdb/storage/record_batch.hpp"

#include <stdexcept>
#include <utility>

namespace atlasdb {

RecordBatch::RecordBatch(Schema schema)
    : schema_(std::move(schema)) {
    columns_.reserve(schema_.column_count());

    for (const auto& definition : schema_.columns()) {
        columns_.emplace_back(definition.type);
    }
}

auto RecordBatch::schema() const noexcept -> const Schema& {
    return schema_;
}

auto RecordBatch::column_count() const noexcept -> std::size_t {
    return columns_.size();
}

auto RecordBatch::row_count() const noexcept -> std::size_t {
    return row_count_;
}

auto RecordBatch::empty() const noexcept -> bool {
    return row_count_ == 0;
}

void RecordBatch::reserve_rows(std::size_t capacity) {
    for (auto& column_vector : columns_) {
        column_vector.reserve(capacity);
    }
}

void RecordBatch::validate_row(const std::vector<Value>& row) const {
    if (row.size() != columns_.size()) {
        throw std::invalid_argument(
            "Row width does not match RecordBatch column count");
    }

    for (std::size_t index = 0; index < row.size(); ++index) {
        if (!row[index].is_compatible_with(columns_[index].type())) {
            throw std::invalid_argument(
                "Row contains a value incompatible with its column type");
        }
    }
}

void RecordBatch::append_row(const std::vector<Value>& row) {
    validate_row(row);

    // Reserve before mutation so allocation failures during reservation
    // do not leave columns with different lengths.
    reserve_rows(row_count_ + 1);

    for (std::size_t index = 0; index < row.size(); ++index) {
        columns_[index].append(row[index]);
    }

    ++row_count_;
}

void RecordBatch::append_row(std::vector<Value>&& row) {
    validate_row(row);
    reserve_rows(row_count_ + 1);

    for (std::size_t index = 0; index < row.size(); ++index) {
        columns_[index].append(std::move(row[index]));
    }

    ++row_count_;
}

void RecordBatch::clear() noexcept {
    for (auto& column_vector : columns_) {
        column_vector.clear();
    }

    row_count_ = 0;
}

auto RecordBatch::column(std::size_t index) const
    -> const ColumnVector& {
    return columns_.at(index);
}

auto RecordBatch::column(std::size_t index) -> ColumnVector& {
    return columns_.at(index);
}

auto RecordBatch::value_at(
    std::size_t row_index,
    std::size_t column_index) const -> const Value& {
    if (row_index >= row_count_) {
        throw std::out_of_range("RecordBatch row index out of range");
    }

    return columns_.at(column_index).at(row_index);
}

}  // namespace atlasdb