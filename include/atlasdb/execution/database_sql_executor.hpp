#pragma once

#include "atlasdb/catalog/database_catalog.hpp"
#include "atlasdb/execution/sql_executor.hpp"
#include "atlasdb/parser/parser.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace atlasdb {

struct DatabaseExecutionResult {
    StatementKind kind{StatementKind::Select};
    std::optional<QueryResult> query_result;
    std::size_t affected_rows{0};
    std::string message;
};

class DatabaseSqlExecutor {
public:
    [[nodiscard]] static auto execute(
        std::string_view sql,
        DatabaseCatalog& catalog) -> DatabaseExecutionResult;
};

}  // namespace atlasdb
