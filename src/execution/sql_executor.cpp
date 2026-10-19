#include "atlasdb/execution/sql_executor.hpp"

#include "atlasdb/execution/query_planner.hpp"
#include "atlasdb/parser/parser.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace atlasdb {

auto SqlExecutor::execute(
    std::string_view sql,
    std::string_view table_name,
    const Table& table
) -> QueryResult {
    Parser parser(sql);
    auto query = parser.parse();

    auto plan = QueryPlanner::plan(query, table_name, table);

    QueryResult result(plan->schema());
    std::vector<Value> row;

    while (true) {
        row.clear();

        if (!plan->next(row)) {
            break;
        }

        result.rows.push_back(row);
    }

    return result;
}

auto SqlExecutor::execute(
    std::string_view sql,
    std::string_view left_name,
    const Table& left,
    std::string_view right_name,
    const Table& right
) -> QueryResult {
    Parser parser(sql);
    const auto query = parser.parse();

    auto plan = QueryPlanner::plan(
        query, left_name, left, right_name, right);

    QueryResult result(plan->schema());
    std::vector<Value> row;
    while (plan->next(row)) {
        result.rows.push_back(row);
        row.clear();
    }
    return result;
}

}  // namespace atlasdb