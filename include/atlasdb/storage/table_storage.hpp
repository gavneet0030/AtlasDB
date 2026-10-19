#pragma once

#include <filesystem>

#include "atlasdb/storage/table.hpp"

namespace atlasdb {

class TableStorage {
public:
    // Save a table to a versioned local file.
    static void save(const Table& table, const std::filesystem::path& path);

    // Load and validate a table from a versioned local file.
    [[nodiscard]] static auto load(const std::filesystem::path& path) -> Table;
};

}  // namespace atlasdb