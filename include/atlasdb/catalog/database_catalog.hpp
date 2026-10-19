#pragma once

#include "atlasdb/storage/table.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atlasdb {

class DatabaseCatalog {
public:
    Table& create_table(std::string name, Schema schema);

    bool contains_table(std::string_view name) const;
    Table& get_table(std::string_view name);
    const Table& get_table(std::string_view name) const;

    std::size_t table_count() const noexcept;
    std::vector<std::string> table_names() const;

private:
    static std::string normalize_name(std::string_view name);

    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
};

}  // namespace atlasdb
