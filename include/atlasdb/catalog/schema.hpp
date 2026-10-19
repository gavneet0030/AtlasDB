#pragma once

#include "atlasdb/types/data_type.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atlasdb {

struct Column {
    std::string name;
    DataType type;
};

class Schema {
public:
    explicit Schema(std::vector<Column> columns);

    [[nodiscard]] auto columns() const noexcept -> const std::vector<Column>&;
    [[nodiscard]] auto column_count() const noexcept -> std::size_t;
    [[nodiscard]] auto find_column(std::string_view name) const noexcept
        -> std::optional<std::size_t>;
    [[nodiscard]] auto column_at(std::size_t index) const -> const Column&;

private:
    std::vector<Column> columns_;
};

}  // namespace atlasdb
