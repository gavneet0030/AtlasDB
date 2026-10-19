#pragma once

#include "atlasdb/types/data_type.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace atlasdb {

class Value {
public:
    using Storage = std::variant<
        std::monostate,
        std::int64_t,
        double,
        bool,
        std::string
    >;

    Value() noexcept = default;
    Value(std::int64_t value);
    Value(int value);
    Value(double value);
    Value(bool value);
    Value(std::string value);
    Value(const char* value);

    [[nodiscard]] static auto null() noexcept -> Value;

    [[nodiscard]] auto is_null() const noexcept -> bool;
    [[nodiscard]] auto data_type() const noexcept -> std::optional<DataType>;
    [[nodiscard]] auto is_compatible_with(DataType expected) const noexcept -> bool;
    [[nodiscard]] auto to_string() const -> std::string;
    [[nodiscard]] auto storage() const noexcept -> const Storage&;

    template <typename T>
    [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(storage_);
    }

private:
    explicit Value(std::monostate) noexcept;

    Storage storage_{};
};

}  // namespace atlasdb
