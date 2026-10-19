#include "atlasdb/execution/query_planner.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace atlasdb {
namespace {

auto make_users_table() -> Table {
    Schema schema(std::vector<Column>{
        {"id", DataType::Int64},
        {"name", DataType::String},
        {"active", DataType::Boolean}
    });

    Table table(std::move(schema));

    table.append_row({
        Value(std::int64_t{1}), Value("Aman"), Value(true)
    });
    table.append_row({
        Value(std::int64_t{2}), Value("Priya"), Value(false)
    });
    table.append_row({
        Value(std::int64_t{3}), Value("Rahul"), Value(true)
    });
    table.append_row({
        Value(std::int64_t{4}), Value("O'Brien"), Value(true)
    });

    return table;
}

auto run_query(
    std::string_view sql,
    const Table& table
) -> std::unique_ptr<ExecutionOperator> {
    Parser parser(sql);
    const auto query = parser.parse();
    return QueryPlanner::plan(query, "users", table);
}

TEST(QueryPlannerTest, FiltersSortsThenProjects) {
    const auto table = make_users_table();

    auto plan = run_query(
        "SELECT name FROM users WHERE id > 1 ORDER BY id DESC;",
        table);

    EXPECT_EQ(plan->schema().column_count(), 1U);
    EXPECT_EQ(plan->schema().column_at(0).name, "name");

    std::vector<Value> row;

    ASSERT_TRUE(plan->next(row));
    EXPECT_EQ(row[0].as<std::string>(), "O'Brien");

    ASSERT_TRUE(plan->next(row));
    EXPECT_EQ(row[0].as<std::string>(), "Rahul");

    ASSERT_TRUE(plan->next(row));
    EXPECT_EQ(row[0].as<std::string>(), "Priya");

    EXPECT_FALSE(plan->next(row));
    EXPECT_TRUE(row.empty());
}

TEST(QueryPlannerTest, SelectStarReturnsAllColumns) {
    const auto table = make_users_table();
    auto plan = run_query("SELECT * FROM users;", table);

    EXPECT_EQ(plan->schema().column_count(), 3U);

    std::vector<Value> row;
    ASSERT_TRUE(plan->next(row));
    ASSERT_EQ(row.size(), 3U);
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);
    EXPECT_EQ(row[1].as<std::string>(), "Aman");
}

TEST(QueryPlannerTest, SupportsBooleanAndCompoundPredicates) {
    const auto table = make_users_table();

    auto plan = run_query(
        "SELECT name FROM users "
        "WHERE active = TRUE AND id > 1 ORDER BY id ASC;",
        table);

    std::vector<Value> row;

    ASSERT_TRUE(plan->next(row));
    EXPECT_EQ(row[0].as<std::string>(), "Rahul");

    ASSERT_TRUE(plan->next(row));
    EXPECT_EQ(row[0].as<std::string>(), "O'Brien");

    EXPECT_FALSE(plan->next(row));
}

TEST(QueryPlannerTest, SupportsEscapedStringLiterals) {
    const auto table = make_users_table();

    auto plan = run_query(
        "SELECT name FROM users WHERE name = 'O''Brien';",
        table);

    std::vector<Value> row;
    ASSERT_TRUE(plan->next(row));
    EXPECT_EQ(row[0].as<std::string>(), "O'Brien");
    EXPECT_FALSE(plan->next(row));
}

TEST(QueryPlannerTest, ResolvesColumnNamesCaseInsensitively) {
    const auto table = make_users_table();

    auto plan = run_query(
        "SELECT NAME FROM USERS WHERE ID = 1;",
        table);

    std::vector<Value> row;
    ASSERT_TRUE(plan->next(row));
    EXPECT_EQ(row[0].as<std::string>(), "Aman");
    EXPECT_FALSE(plan->next(row));
}

TEST(QueryPlannerTest, RejectsUnknownSelectedColumn) {
    const auto table = make_users_table();

    EXPECT_THROW(
        run_query("SELECT email FROM users;", table),
        std::invalid_argument);
}

TEST(QueryPlannerTest, RejectsUnknownWhereColumn) {
    const auto table = make_users_table();

    EXPECT_THROW(
        run_query("SELECT name FROM users WHERE missing = 1;", table),
        std::invalid_argument);
}

TEST(QueryPlannerTest, RejectsUnknownOrderByColumn) {
    const auto table = make_users_table();

    EXPECT_THROW(
        run_query("SELECT name FROM users ORDER BY missing;", table),
        std::invalid_argument);
}

TEST(QueryPlannerTest, RejectsMismatchedTableName) {
    const auto table = make_users_table();
    Parser parser("SELECT * FROM orders;");
    const auto query = parser.parse();

    EXPECT_THROW(
        QueryPlanner::plan(query, "users", table),
        std::invalid_argument);
}

TEST(QueryPlannerTest, SupportsNotExpression) {
    const auto table = make_users_table();

    auto plan = run_query(
        "SELECT name FROM users WHERE NOT active;",
        table);

    std::vector<Value> row;
    ASSERT_TRUE(plan->next(row));
    EXPECT_EQ(row[0].as<std::string>(), "Priya");
    EXPECT_FALSE(plan->next(row));
}

}  // namespace
}  // namespace atlasdb
