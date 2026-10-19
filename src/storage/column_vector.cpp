#include "atlasdb/storage/column_vector.hpp"

#include <stdexcept>
#include <utility>

namespace atlasdb {

ColumnVector::ColumnVector(DataType type)
    : type_(type) {}

auto ColumnVector::type() const noexcept -> DataType {
    return type_;
}

auto ColumnVector::size() const noexcept -> std::size_t {
    return values_.size();
}

auto ColumnVector::empty() const noexcept -> bool {
    return values_.empty();
}

void ColumnVector::reserve(std::size_t capacity) {
    values_.reserve(capacity);
}

void ColumnVector::append(const Value& value) {
    if (!value.is_compatible_with(type_)) {
        throw std::invalid_argument(
            "Value type is incompatible with ColumnVector type");
    }

    values_.push_back(value);
}

void ColumnVector::append(Value&& value) {
    if (!value.is_compatible_with(type_)) {
        throw std::invalid_argument(
            "Value type is incompatible with ColumnVector type");
    }

    values_.push_back(std::move(value));
}

void ColumnVector::clear() noexcept {
    values_.clear();
}

auto ColumnVector::at(std::size_t index) const -> const Value& {
    return values_.at(index);
}

auto ColumnVector::values() const noexcept -> const std::vector<Value>& {
    return values_;
}

}  // namespace atlasdb