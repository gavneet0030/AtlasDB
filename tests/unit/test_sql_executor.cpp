#include "atlasdb/execution/sql_executor.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
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

TEST(SqlExecutorTest, ExecutesSelectStar) {
    const auto table = make_users_table();

    const auto result = SqlExecutor::execute(
        "SELECT * FROM users;", "users", table);

    ASSERT_EQ(result.schema.column_count(), 3U);
    ASSERT_EQ(result.rows.size(), 4U);
    EXPECT_EQ(result.rows[0][0].as<std::int64_t>(), 1);
    EXPECT_EQ(result.rows[0][1].as<std::string>(), "Aman");
    EXPECT_TRUE(result.rows[0][2].as<bool>());
}

TEST(SqlExecutorTest, ExecutesFilterSortAndProjection) {
    const auto table = make_users_table();

    const auto result = SqlExecutor::execute(
        "SELECT name FROM users WHERE id > 1 ORDER BY id DESC;",
        "users",
        table);

    ASSERT_EQ(result.schema.column_count(), 1U);
    EXPECT_EQ(result.schema.column_at(0).name, "name");
    ASSERT_EQ(result.rows.size(), 3U);

    EXPECT_EQ(result.rows[0][0].as<std::string>(), "O'Brien");
    EXPECT_EQ(result.rows[1][0].as<std::string>(), "Rahul");
    EXPECT_EQ(result.rows[2][0].as<std::string>(), "Priya");
}

TEST(SqlExecutorTest, ReturnsEmptyResultWhenNoRowsMatch) {
    const auto table = make_users_table();

    const auto result = SqlExecutor::execute(
        "SELECT name FROM users WHERE id > 100;",
        "users",
        table);

    EXPECT_EQ(result.schema.column_count(), 1U);
    EXPECT_TRUE(result.rows.empty());
}

TEST(SqlExecutorTest, SupportsCompoundPredicates) {
    const auto table = make_users_table();

    const auto result = SqlExecutor::execute(
        "SELECT name FROM users "
        "WHERE active = TRUE AND id > 1 ORDER BY id ASC;",
        "users",
        table);

    ASSERT_EQ(result.rows.size(), 2U);
    EXPECT_EQ(result.rows[0][0].as<std::string>(), "Rahul");
    EXPECT_EQ(result.rows[1][0].as<std::string>(), "O'Brien");
}

TEST(SqlExecutorTest, SupportsEscapedStringLiteral) {
    const auto table = make_users_table();

    const auto result = SqlExecutor::execute(
        "SELECT name FROM users WHERE name = 'O''Brien';",
        "users",
        table);

    ASSERT_EQ(result.rows.size(), 1U);
    EXPECT_EQ(result.rows[0][0].as<std::string>(), "O'Brien");
}

TEST(SqlExecutorTest, RejectsUnknownColumn) {
    const auto table = make_users_table();

    EXPECT_THROW(
        SqlExecutor::execute(
            "SELECT email FROM users;", "users", table),
        std::invalid_argument);
}

TEST(SqlExecutorTest, RejectsMismatchedTableName) {
    const auto table = make_users_table();

    EXPECT_THROW(
        SqlExecutor::execute(
            "SELECT * FROM orders;", "users", table),
        std::invalid_argument);
}

}  // namespace
}  // namespace atlasdb