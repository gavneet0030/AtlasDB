#pragma once

#include <string_view>

namespace atlasdb {

[[nodiscard]] auto version() noexcept -> std::string_view;

}  // namespace atlasdb