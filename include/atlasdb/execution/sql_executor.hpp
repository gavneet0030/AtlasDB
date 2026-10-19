#pragma once

#include "atlasdb/catalog/schema.hpp"
#include "atlasdb/storage/table.hpp"
#include "atlasdb/types/value.hpp"

#include <string_view>
#include <vector>

namespace atlasdb {

struct QueryResult {
    explicit QueryResult(const Schema& result_schema)
        : schema(result_schema) {}

    Schema schema;
    std::vector<std::vector<Value>> rows;
};

class SqlExecutor {
public:
    [[nodiscard]] static auto execute(
        std::string_view sql,
        std::string_view table_name,
        const Table& table
    ) -> QueryResult;

    [[nodiscard]] static auto execute(
        std::string_view sql,
        std::string_view left_name,
        const Table& left,
        std::string_view right_name,
        const Table& right
    ) -> QueryResult;
};

}  // namespace atlasdb