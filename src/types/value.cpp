#include "atlasdb/types/value.hpp"

#include <stdexcept>
#include <utility>

namespace atlasdb {

Value::Value(std::int64_t value) : storage_(value) {}

Value::Value(int value) : storage_(static_cast<std::int64_t>(value)) {}

Value::Value(double value) : storage_(value) {}

Value::Value(bool value) : storage_(value) {}

Value::Value(std::string value) : storage_(std::move(value)) {}

Value::Value(const char* value)
    : storage_(value == nullptr ? Storage{} : Storage{std::string(value)}) {}

Value::Value(std::monostate) noexcept : storage_(std::monostate{}) {}

auto Value::null() noexcept -> Value {
    return Value(std::monostate{});
}

auto Value::is_null() const noexcept -> bool {
    return std::holds_alternative<std::monostate>(storage_);
}

auto Value::data_type() const noexcept -> std::optional<DataType> {
    if (std::holds_alternative<std::int64_t>(storage_)) {
        return DataType::Int64;
    }
    if (std::holds_alternative<double>(storage_)) {
        return DataType::Double;
    }
    if (std::holds_alternative<bool>(storage_)) {
        return DataType::Boolean;
    }
    if (std::holds_alternative<std::string>(storage_)) {
        return DataType::String;
    }

    return std::nullopt;
}

auto Value::is_compatible_with(DataType expected) const noexcept -> bool {
    const auto actual = data_type();
    return !actual.has_value() || *actual == expected;
}

auto Value::to_string() const -> std::string {
    if (is_null()) {
        return "NULL";
    }
    if (const auto* value = std::get_if<std::int64_t>(&storage_)) {
        return std::to_string(*value);
    }
    if (const auto* value = std::get_if<double>(&storage_)) {
        return std::to_string(*value);
    }
    if (const auto* value = std::get_if<bool>(&storage_)) {
        return *value ? "TRUE" : "FALSE";
    }
    return std::get<std::string>(storage_);
}

auto Value::storage() const noexcept -> const Storage& {
    return storage_;
}

}  // namespace atlasdb
