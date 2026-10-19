#include "atlasdb/catalog/database_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace atlasdb {

std::string DatabaseCatalog::normalize_name(std::string_view name) {
    if (name.empty()) {
        throw std::invalid_argument("Table name cannot be empty");
    }

    std::string normalized;
    normalized.reserve(name.size());

    for (unsigned char ch : name) {
        normalized.push_back(static_cast<char>(std::tolower(ch)));
    }

    return normalized;
}

Table& DatabaseCatalog::create_table(std::string name, Schema schema) {
    const std::string key = normalize_name(name);

    if (tables_.contains(key)) {
        throw std::invalid_argument("Table already exists: " + name);
    }

    auto table = std::make_unique<Table>(std::move(schema));
    Table& result = *table;
    tables_.emplace(key, std::move(table));
    return result;
}

bool DatabaseCatalog::contains_table(std::string_view name) const {
    return tables_.contains(normalize_name(name));
}

Table& DatabaseCatalog::get_table(std::string_view name) {
    const std::string key = normalize_name(name);
    const auto it = tables_.find(key);

    if (it == tables_.end()) {
        throw std::out_of_range("Table not found: " + std::string(name));
    }

    return *it->second;
}

const Table& DatabaseCatalog::get_table(std::string_view name) const {
    const std::string key = normalize_name(name);
    const auto it = tables_.find(key);

    if (it == tables_.end()) {
        throw std::out_of_range("Table not found: " + std::string(name));
    }

    return *it->second;
}

std::size_t DatabaseCatalog::table_count() const noexcept {
    return tables_.size();
}

std::vector<std::string> DatabaseCatalog::table_names() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());

    for (const auto& [name, table] : tables_) {
        (void)table;
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace atlasdb
