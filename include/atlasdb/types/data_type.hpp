#pragma once

#include <string_view>

namespace atlasdb {

enum class DataType {
    Int64,
    Double,
    Boolean,
    String
};

[[nodiscard]] auto to_string(DataType type) noexcept -> std::string_view;

}  // namespace atlasdb
