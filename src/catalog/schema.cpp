#include "atlasdb/catalog/schema.hpp"

#include <stdexcept>
#include <utility>

namespace atlasdb {

Schema::Schema(std::vector<Column> columns)
    : columns_(std::move(columns)) {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name.empty()) {
            throw std::invalid_argument("Column name must not be empty");
        }

        for (std::size_t j = 0; j < i; ++j) {
            if (columns_[i].name == columns_[j].name) {
                throw std::invalid_argument(
                    "Duplicate column name: " + columns_[i].name
                );
            }
        }
    }
}

auto Schema::columns() const noexcept -> const std::vector<Column>& {
    return columns_;
}

auto Schema::column_count() const noexcept -> std::size_t {
    return columns_.size();
}

auto Schema::find_column(std::string_view name) const noexcept
    -> std::optional<std::size_t> {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name) {
            return i;
        }
    }

    return std::nullopt;
}

auto Schema::column_at(std::size_t index) const -> const Column& {
    return columns_.at(index);
}

}  // namespace atlasdb
