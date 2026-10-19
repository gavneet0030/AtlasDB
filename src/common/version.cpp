#include "atlasdb/common/version.hpp"

namespace atlasdb {

auto version() noexcept -> std::string_view {
    return "0.1.0";
}

}  // namespace atlasdb