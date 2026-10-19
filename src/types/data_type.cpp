#include "atlasdb/types/data_type.hpp"

namespace atlasdb {

auto to_string(DataType type) noexcept -> std::string_view {
    switch (type) {
        case DataType::Int64:
            return "INT64";
        case DataType::Double:
            return "DOUBLE";
        case DataType::Boolean:
            return "BOOLEAN";
        case DataType::String:
            return "STRING";
    }

    return "UNKNOWN";
}

}  // namespace atlasdb
