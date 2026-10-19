#include "atlasdb/storage/table.hpp"

#include <stdexcept>
#include <utility>

namespace atlasdb {

Table::Table(Schema schema)
    : schema_(std::move(schema)) {}

auto Table::schema() const noexcept -> const Schema& {
    return schema_;
}

auto Table::row_count() const noexcept -> std::size_t {
    return row_count_;
}

auto Table::batch_count() const noexcept -> std::size_t {
    return batches_.size();
}

auto Table::empty() const noexcept -> bool {
    return row_count_ == 0;
}

auto Table::schemas_match(const Schema& other) const noexcept -> bool {
    if (schema_.column_count() != other.column_count()) {
        return false;
    }

    for (std::size_t index = 0; index < schema_.column_count(); ++index) {
        const auto& left = schema_.columns()[index];
        const auto& right = other.columns()[index];

        if (left.name != right.name || left.type != right.type) {
            return false;
        }
    }

    return true;
}

void Table::append_row(const std::vector<Value>& row) {
    if (batches_.empty()) {
        batches_.emplace_back(schema_);
    }

    batches_.back().append_row(row);
    ++row_count_;
}

void Table::append_row(std::vector<Value>&& row) {
    if (batches_.empty()) {
        batches_.emplace_back(schema_);
    }

    batches_.back().append_row(std::move(row));
    ++row_count_;
}

void Table::append_batch(RecordBatch batch_value) {
    if (!schemas_match(batch_value.schema())) {
        throw std::invalid_argument(
            "RecordBatch schema does not match Table schema");
    }

    const auto incoming_rows = batch_value.row_count();

    // Move the complete batch into the table only after validation.
    batches_.push_back(std::move(batch_value));
    row_count_ += incoming_rows;
}

auto Table::value_at(
    std::size_t row_index,
    std::size_t column_index) const -> const Value& {
    if (row_index >= row_count_) {
        throw std::out_of_range("Table row index out of range");
    }

    auto remaining = row_index;

    for (const auto& current_batch : batches_) {
        if (remaining < current_batch.row_count()) {
            return current_batch.value_at(remaining, column_index);
        }

        remaining -= current_batch.row_count();
    }

    throw std::out_of_range("Table row index out of range");
}

auto Table::batch(std::size_t index) const -> const RecordBatch& {
    return batches_.at(index);
}

void Table::clear() noexcept {
    batches_.clear();
    row_count_ = 0;
}

}  // namespace atlasdb