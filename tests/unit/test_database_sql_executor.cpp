#include "atlasdb/execution/database_sql_executor.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace atlasdb {
namespace {

auto run_sql(std::string_view sql, DatabaseCatalog& catalog)
    -> DatabaseExecutionResult {
    return DatabaseSqlExecutor::execute(sql, catalog);
}

TEST(DatabaseSqlExecutorTest, CreatesIndependentTables) {
    DatabaseCatalog catalog;

    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql("CREATE TABLE orders (id INT64, total DOUBLE);", catalog);

    EXPECT_TRUE(catalog.contains_table("users"));
    EXPECT_TRUE(catalog.contains_table("orders"));
    EXPECT_EQ(catalog.table_count(), 2U);
    EXPECT_EQ(catalog.get_table("users").schema().column_count(), 2U);
    EXPECT_EQ(catalog.get_table("orders").schema().column_count(), 2U);
}

TEST(DatabaseSqlExecutorTest, RejectsDuplicateTable) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64);", catalog);

    EXPECT_THROW(
        run_sql("CREATE TABLE USERS (id INT64);", catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, InsertsAndSelectsMultipleRows) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING, active BOOLEAN);",
            catalog);

    const auto inserted = run_sql(
        "INSERT INTO users VALUES "
        "(1, 'Aman', TRUE), (2, 'Priya', FALSE);",
        catalog);

    EXPECT_EQ(inserted.affected_rows, 2U);
    EXPECT_EQ(catalog.get_table("users").row_count(), 2U);

    const auto selected = run_sql(
        "SELECT name FROM users WHERE active = TRUE;", catalog);

    ASSERT_TRUE(selected.query_result.has_value());
    ASSERT_EQ(selected.query_result->rows.size(), 1U);
    EXPECT_EQ(
        selected.query_result->rows[0][0].as<std::string>(), "Aman");
}

TEST(DatabaseSqlExecutorTest, SupportsInsertColumnListAndNullDefaults) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING, active BOOLEAN);",
            catalog);

    run_sql("INSERT INTO users (name, id) VALUES ('Riya', 7);", catalog);

    const Table& table = catalog.get_table("users");
    ASSERT_EQ(table.row_count(), 1U);
    EXPECT_EQ(table.value_at(0, 0).as<std::int64_t>(), 7);
    EXPECT_EQ(table.value_at(0, 1).as<std::string>(), "Riya");
    EXPECT_TRUE(table.value_at(0, 2).is_null());
}

TEST(DatabaseSqlExecutorTest, ResolvesTableNamesCaseInsensitively) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE Users (id INT64);", catalog);
    run_sql("INSERT INTO USERS VALUES (9);", catalog);

    const auto result = run_sql("SELECT * FROM users;", catalog);

    ASSERT_TRUE(result.query_result.has_value());
    ASSERT_EQ(result.query_result->rows.size(), 1U);
    EXPECT_EQ(result.query_result->rows[0][0].as<std::int64_t>(), 9);
}

TEST(DatabaseSqlExecutorTest, RejectsMissingTable) {
    DatabaseCatalog catalog;

    EXPECT_THROW(
        run_sql("INSERT INTO missing VALUES (1);", catalog),
        std::out_of_range);

    EXPECT_THROW(
        run_sql("SELECT * FROM missing;", catalog),
        std::out_of_range);
}

TEST(DatabaseSqlExecutorTest, RejectsTypeMismatchWithoutPartialInsert) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, active BOOLEAN);", catalog);

    EXPECT_THROW(
        run_sql("INSERT INTO users VALUES (1, TRUE), (2, 'not_boolean');",
                catalog),
        std::invalid_argument);

    EXPECT_EQ(catalog.get_table("users").row_count(), 0U);
}

TEST(DatabaseSqlExecutorTest, RejectsUnknownAndDuplicateInsertColumns) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);

    EXPECT_THROW(
        run_sql("INSERT INTO users (missing) VALUES (1);", catalog),
        std::invalid_argument);

    EXPECT_THROW(
        run_sql("INSERT INTO users (id, ID) VALUES (1, 2);", catalog),
        std::invalid_argument);

    EXPECT_EQ(catalog.get_table("users").row_count(), 0U);
}

TEST(DatabaseSqlExecutorTest, RejectsIncorrectInsertRowWidth) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);

    EXPECT_THROW(
        run_sql("INSERT INTO users VALUES (1);", catalog),
        std::invalid_argument);

    EXPECT_EQ(catalog.get_table("users").row_count(), 0U);
}

TEST(DatabaseSqlExecutorTest, PreservesEscapedStringsAndNumericTypes) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE products (name STRING, price DOUBLE);", catalog);
    run_sql("INSERT INTO products VALUES ('O''Brien', 12.5);", catalog);

    const Table& table = catalog.get_table("products");
    ASSERT_EQ(table.row_count(), 1U);
    EXPECT_EQ(table.value_at(0, 0).as<std::string>(), "O'Brien");
    EXPECT_DOUBLE_EQ(table.value_at(0, 1).as<double>(), 12.5);
}


TEST(DatabaseSqlExecutorTest, ExecutesInclusiveComparisonOperators) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE scores (score INT64);", catalog);
    run_sql("INSERT INTO scores VALUES (10), (20), (30);", catalog);

    const auto less_equal =
        run_sql("SELECT score FROM scores WHERE score <= 20;", catalog);
    ASSERT_TRUE(less_equal.query_result.has_value());
    ASSERT_EQ(less_equal.query_result->rows.size(), 2U);
    EXPECT_EQ(less_equal.query_result->rows[0][0].as<std::int64_t>(), 10);
    EXPECT_EQ(less_equal.query_result->rows[1][0].as<std::int64_t>(), 20);

    const auto greater_equal =
        run_sql("SELECT score FROM scores WHERE score >= 20;", catalog);
    ASSERT_TRUE(greater_equal.query_result.has_value());
    ASSERT_EQ(greater_equal.query_result->rows.size(), 2U);
    EXPECT_EQ(greater_equal.query_result->rows[0][0].as<std::int64_t>(), 20);
    EXPECT_EQ(greater_equal.query_result->rows[1][0].as<std::int64_t>(), 30);
}

TEST(DatabaseSqlExecutorTest, ExecutesAggregateFunctionsThroughSql) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE employees (salary INT64);", catalog);
    run_sql("INSERT INTO employees VALUES (100), (200), (300);", catalog);

    const auto result = run_sql(
        "SELECT COUNT(*), SUM(salary), AVG(salary), "
        "MIN(salary), MAX(salary) FROM employees;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 1U);
    ASSERT_EQ(query.rows[0].size(), 5U);

    EXPECT_EQ(query.rows[0][0].as<std::int64_t>(), 3);
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 600);
    EXPECT_DOUBLE_EQ(query.rows[0][2].as<double>(), 200.0);
    EXPECT_EQ(query.rows[0][3].as<std::int64_t>(), 100);
    EXPECT_EQ(query.rows[0][4].as<std::int64_t>(), 300);
}

TEST(DatabaseSqlExecutorTest, FiltersRowsBeforeAggregation) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE employees (salary INT64);", catalog);
    run_sql("INSERT INTO employees VALUES (100), (200), (300);", catalog);

    const auto result = run_sql(
        "SELECT COUNT(*), SUM(salary) FROM employees WHERE salary > 100;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    ASSERT_EQ(result.query_result->rows.size(), 1U);
    ASSERT_EQ(result.query_result->rows[0].size(), 2U);

    EXPECT_EQ(result.query_result->rows[0][0].as<std::int64_t>(), 2);
    EXPECT_EQ(result.query_result->rows[0][1].as<std::int64_t>(), 500);
}

TEST(DatabaseSqlExecutorTest, AggregatesEmptyTable) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE employees (salary INT64);", catalog);

    const auto result = run_sql(
        "SELECT COUNT(*), SUM(salary), AVG(salary) FROM employees;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    ASSERT_EQ(result.query_result->rows.size(), 1U);
    ASSERT_EQ(result.query_result->rows[0].size(), 3U);

    EXPECT_EQ(result.query_result->rows[0][0].as<std::int64_t>(), 0);
    EXPECT_TRUE(result.query_result->rows[0][1].is_null());
    EXPECT_TRUE(result.query_result->rows[0][2].is_null());
}

TEST(DatabaseSqlExecutorTest, RejectsAggregateOnUnknownColumn) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE employees (salary INT64);", catalog);

    EXPECT_THROW(
        run_sql("SELECT SUM(missing) FROM employees;", catalog),
        std::exception);
}

TEST(DatabaseSqlExecutorTest, RejectsStarArgumentForSum) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE employees (salary INT64);", catalog);

    EXPECT_THROW(
        run_sql("SELECT SUM(*) FROM employees;", catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, RejectsOrderByWithAggregate) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE employees (salary INT64);", catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT SUM(salary) FROM employees ORDER BY salary;",
            catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, RejectsAggregateWithJoin) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE employees (id INT64, salary INT64);", catalog);
    run_sql("CREATE TABLE departments (id INT64);", catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT COUNT(*) FROM employees "
            "INNER JOIN departments ON employees.id = departments.id;",
            catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, RejectsMixedAggregateAndOrdinaryColumn) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE employees (id INT64, salary INT64);", catalog);

    EXPECT_THROW(
        run_sql("SELECT id, SUM(salary) FROM employees;", catalog),
        std::exception);
}
TEST(DatabaseSqlExecutorTest, ExecutesInnerJoinWithWhereAndProjection) {
    DatabaseCatalog catalog;
    run_sql(
        "CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql(
        "CREATE TABLE orders (id INT64, user_id INT64, total INT64);",
        catalog);

    run_sql(
        "INSERT INTO users VALUES (1, 'Aman'), (2, 'Priya');",
        catalog);
    run_sql(
        "INSERT INTO orders VALUES "
        "(10, 1, 50), (11, 1, 150), (12, 3, 200);",
        catalog);

    const auto result = run_sql(
        "SELECT users.name, orders.total FROM users "
        "INNER JOIN orders ON users.id = orders.user_id "
        "WHERE orders.total >= 100 ORDER BY orders.total DESC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    ASSERT_EQ(result.query_result->rows.size(), 1U);
    ASSERT_EQ(result.query_result->rows[0].size(), 2U);
    EXPECT_EQ(
        result.query_result->rows[0][0].as<std::string>(), "Aman");
    EXPECT_EQ(
        result.query_result->rows[0][1].as<std::int64_t>(), 150);
}


TEST(DatabaseSqlExecutorTest, ReturnsMultipleRowsForOneToManyJoin) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql("CREATE TABLE orders (id INT64, user_id INT64, total INT64);",
            catalog);
    run_sql("INSERT INTO users VALUES (1, 'Aman');", catalog);
    run_sql(
        "INSERT INTO orders VALUES (10, 1, 50), (11, 1, 150), (12, 1, 250);",
        catalog);

    const auto result = run_sql(
        "SELECT users.name, orders.total FROM users "
        "INNER JOIN orders ON users.id = orders.user_id "
        "ORDER BY orders.total ASC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    ASSERT_EQ(result.query_result->rows.size(), 3U);
    EXPECT_EQ(result.query_result->rows[0][0].as<std::string>(), "Aman");
    EXPECT_EQ(result.query_result->rows[0][1].as<std::int64_t>(), 50);
    EXPECT_EQ(result.query_result->rows[1][1].as<std::int64_t>(), 150);
    EXPECT_EQ(result.query_result->rows[2][1].as<std::int64_t>(), 250);
}

TEST(DatabaseSqlExecutorTest, ReturnsEmptyResultWhenJoinHasNoMatches) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql("CREATE TABLE orders (id INT64, user_id INT64);", catalog);
    run_sql("INSERT INTO users VALUES (1, 'Aman');", catalog);
    run_sql("INSERT INTO orders VALUES (10, 99);", catalog);

    const auto result = run_sql(
        "SELECT users.name FROM users "
        "INNER JOIN orders ON users.id = orders.user_id;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    EXPECT_TRUE(result.query_result->rows.empty());
}

TEST(DatabaseSqlExecutorTest, RejectsJoinWithMissingTable) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT users.name FROM users "
            "INNER JOIN missing ON users.id = missing.id;",
            catalog),
        std::out_of_range);
}

TEST(DatabaseSqlExecutorTest, RejectsAmbiguousUnqualifiedJoinColumn) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql("CREATE TABLE orders (id INT64, user_id INT64);", catalog);
    run_sql("INSERT INTO users VALUES (1, 'Aman');", catalog);
    run_sql("INSERT INTO orders VALUES (10, 1);", catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT id FROM users "
            "INNER JOIN orders ON users.id = orders.user_id;",
            catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, RejectsUnknownQualifierInSelect) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql("CREATE TABLE orders (id INT64, user_id INT64);", catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT wrong_table.id FROM users "
            "INNER JOIN orders ON users.id = orders.user_id;",
            catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, RejectsUnknownQualifierInWhere) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql("CREATE TABLE orders (id INT64, user_id INT64);", catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT users.name FROM users "
            "INNER JOIN orders ON users.id = orders.user_id "
            "WHERE wrong_table.id = 1;",
            catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, RejectsUnknownQualifierInJoinCondition) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql("CREATE TABLE orders (id INT64, user_id INT64);", catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT users.name FROM users "
            "INNER JOIN orders ON wrong_table.id = orders.user_id;",
            catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, RejectsUnknownQualifierInOrderBy) {
    DatabaseCatalog catalog;
    run_sql("CREATE TABLE users (id INT64, name STRING);", catalog);
    run_sql("CREATE TABLE orders (id INT64, user_id INT64);", catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT users.name FROM users "
            "INNER JOIN orders ON users.id = orders.user_id "
            "ORDER BY wrong_table.id;",
            catalog),
        std::invalid_argument);
}
TEST(DatabaseSqlExecutorTest, ExecutesGroupedAggregationThroughSql) {
    DatabaseCatalog catalog;
    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 100), ('Sales', 50), ('Engineering', 200);",
        catalog);

    const auto result = run_sql(
        "SELECT department, COUNT(*), SUM(salary) "
        "FROM employees GROUP BY department;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 2U);

    ASSERT_EQ(query.rows[0].size(), 3U);
    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 2);
    EXPECT_EQ(query.rows[0][2].as<std::int64_t>(), 300);

    ASSERT_EQ(query.rows[1].size(), 3U);
    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Sales");
    EXPECT_EQ(query.rows[1][1].as<std::int64_t>(), 1);
    EXPECT_EQ(query.rows[1][2].as<std::int64_t>(), 50);
}
TEST(DatabaseSqlExecutorTest, GroupsNullKeysThroughSql) {
    DatabaseCatalog catalog;
    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "(NULL, 10), ('Engineering', 20), (NULL, 30);",
        catalog);

    const auto result = run_sql(
        "SELECT department, COUNT(*), SUM(salary) "
        "FROM employees GROUP BY department;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 2U);

    ASSERT_EQ(query.rows[0].size(), 3U);
    EXPECT_TRUE(query.rows[0][0].is_null());
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 2);
    EXPECT_EQ(query.rows[0][2].as<std::int64_t>(), 40);

    ASSERT_EQ(query.rows[1].size(), 3U);
    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[1][1].as<std::int64_t>(), 1);
    EXPECT_EQ(query.rows[1][2].as<std::int64_t>(), 20);
}

TEST(DatabaseSqlExecutorTest, RejectsGroupByWithoutAggregate) {
    DatabaseCatalog catalog;
    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT department FROM employees GROUP BY department;",
            catalog),
        std::invalid_argument);
}
TEST(DatabaseSqlExecutorTest, PreservesInt64PrecisionForGroupedMinAndMax) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE measurements (category STRING, amount INT64);",
        catalog);
    run_sql(
        "INSERT INTO measurements VALUES "
        "('A', 9007199254740993), "
        "('A', 9007199254740992);",
        catalog);

    const auto result = run_sql(
        "SELECT category, MIN(amount), MAX(amount) "
        "FROM measurements GROUP BY category;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 1U);
    ASSERT_EQ(query.rows[0].size(), 3U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "A");
    EXPECT_EQ(
        query.rows[0][1].as<std::int64_t>(),
        INT64_C(9007199254740992));
    EXPECT_EQ(
        query.rows[0][2].as<std::int64_t>(),
        INT64_C(9007199254740993));
}
TEST(DatabaseSqlExecutorTest, OrdersGroupedAggregatesByCountStarDescending) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 100), "
        "('Engineering', 200), "
        "('Sales', 400), "
        "('Support', 150);",
        catalog);

    const auto result = run_sql(
        "SELECT department, COUNT(*) "
        "FROM employees GROUP BY department "
        "ORDER BY COUNT(*) DESC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 3U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 2);
}

TEST(DatabaseSqlExecutorTest, OrdersGroupedAggregatesByAggregateThenGroupColumn) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 100), "
        "('Engineering', 200), "
        "('Sales', 300), "
        "('Support', 100);",
        catalog);

    const auto result = run_sql(
        "SELECT department, SUM(salary) "
        "FROM employees GROUP BY department "
        "ORDER BY SUM(salary) DESC, department ASC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 3U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 300);

    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Sales");
    EXPECT_EQ(query.rows[1][1].as<std::int64_t>(), 300);

    EXPECT_EQ(query.rows[2][0].as<std::string>(), "Support");
    EXPECT_EQ(query.rows[2][1].as<std::int64_t>(), 100);
}

TEST(DatabaseSqlExecutorTest, RejectsOrderByAggregateNotSelected) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 100), "
        "('Sales', 400);",
        catalog);

    EXPECT_THROW(
        run_sql(
            "SELECT department, SUM(salary) "
            "FROM employees GROUP BY department "
            "ORDER BY MAX(salary) DESC;",
            catalog),
        std::invalid_argument);
}

TEST(DatabaseSqlExecutorTest, OrdersByAverageDescendingCaseInsensitively) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 10), "
        "('Engineering', 20), "
        "('Sales', 100), "
        "('Support', 30), "
        "('Support', 50);",
        catalog);

    const auto result = run_sql(
        "SELECT department, AVG(salary) "
        "FROM employees GROUP BY department "
        "ORDER BY avg(salary) DESC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 3U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Sales");
    EXPECT_DOUBLE_EQ(query.rows[0][1].as<double>(), 100.0);

    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Support");
    EXPECT_DOUBLE_EQ(query.rows[1][1].as<double>(), 40.0);

    EXPECT_EQ(query.rows[2][0].as<std::string>(), "Engineering");
    EXPECT_DOUBLE_EQ(query.rows[2][1].as<double>(), 15.0);
}

TEST(DatabaseSqlExecutorTest, OrdersByMultipleAggregateKeys) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Sales', 100), "
        "('Sales', 300), "
        "('Support', 100), "
        "('Support', 100), "
        "('Support', 200), "
        "('Engineering', 50), "
        "('Engineering', 150);",
        catalog);

    const auto result = run_sql(
        "SELECT department, SUM(salary), AVG(salary) "
        "FROM employees GROUP BY department "
        "ORDER BY SUM(salary) DESC, AVG(salary) ASC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 3U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Support");
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 400);
    EXPECT_DOUBLE_EQ(query.rows[0][2].as<double>(), 400.0 / 3.0);

    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Sales");
    EXPECT_EQ(query.rows[1][1].as<std::int64_t>(), 400);
    EXPECT_DOUBLE_EQ(query.rows[1][2].as<double>(), 200.0);

    EXPECT_EQ(query.rows[2][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[2][1].as<std::int64_t>(), 200);
    EXPECT_DOUBLE_EQ(query.rows[2][2].as<double>(), 100.0);
}
TEST(DatabaseSqlExecutorTest, OrdersByAverageDescendingWithNullValues) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 10), "
        "('Engineering', NULL), "
        "('Engineering', 20), "
        "('Sales', 100), "
        "('Sales', NULL), "
        "('Support', 30), "
        "('Support', 50);",
        catalog);

    const auto result = run_sql(
        "SELECT department, AVG(salary) "
        "FROM employees GROUP BY department "
        "ORDER BY AVG(salary) DESC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 3U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Sales");
    EXPECT_DOUBLE_EQ(query.rows[0][1].as<double>(), 100.0);

    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Support");
    EXPECT_DOUBLE_EQ(query.rows[1][1].as<double>(), 40.0);

    EXPECT_EQ(query.rows[2][0].as<std::string>(), "Engineering");
    EXPECT_DOUBLE_EQ(query.rows[2][1].as<double>(), 15.0);
}
TEST(DatabaseSqlExecutorTest, OrdersByCountColumnWithNullValues) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 100), "
        "('Engineering', NULL), "
        "('Sales', 200), "
        "('Sales', 300), "
        "('Support', NULL);",
        catalog);

    const auto result = run_sql(
        "SELECT department, COUNT(salary), SUM(salary) "
        "FROM employees GROUP BY department "
        "ORDER BY COUNT(salary) DESC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 3U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Sales");
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 2);
    EXPECT_EQ(query.rows[0][2].as<std::int64_t>(), 500);

    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[1][1].as<std::int64_t>(), 1);
    EXPECT_EQ(query.rows[1][2].as<std::int64_t>(), 100);

    EXPECT_EQ(query.rows[2][0].as<std::string>(), "Support");
    EXPECT_EQ(query.rows[2][1].as<std::int64_t>(), 0);
    EXPECT_TRUE(query.rows[2][2].is_null());
}

TEST(DatabaseSqlExecutorTest, OrdersBySecondSelectedAggregate) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 100), "
        "('Engineering', 200), "
        "('Sales', 400), "
        "('Support', 50), "
        "('Support', 60), "
        "('Support', 70);",
        catalog);

    const auto result = run_sql(
        "SELECT department, SUM(salary), COUNT(*) "
        "FROM employees GROUP BY department "
        "ORDER BY COUNT(*) DESC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 3U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Support");
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 180);
    EXPECT_EQ(query.rows[0][2].as<std::int64_t>(), 3);

    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[1][1].as<std::int64_t>(), 300);
    EXPECT_EQ(query.rows[1][2].as<std::int64_t>(), 2);

    EXPECT_EQ(query.rows[2][0].as<std::string>(), "Sales");
    EXPECT_EQ(query.rows[2][1].as<std::int64_t>(), 400);
    EXPECT_EQ(query.rows[2][2].as<std::int64_t>(), 1);
}

TEST(DatabaseSqlExecutorTest, OrdersGroupedAggregatesByAggregateExpressionDescending) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 100), "
        "('Engineering', 200), "
        "('Sales', 400);",
        catalog);

    const auto result = run_sql(
        "SELECT department, SUM(salary) "
        "FROM employees GROUP BY department "
        "ORDER BY SUM(salary) DESC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 2U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Sales");
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 400);

    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[1][1].as<std::int64_t>(), 300);
}

TEST(DatabaseSqlExecutorTest, OrdersGroupedAggregatesByGroupColumnDescending) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 100), "
        "('Engineering', 200), "
        "('Sales', 400);",
        catalog);

    const auto result = run_sql(
        "SELECT department, SUM(salary) "
        "FROM employees GROUP BY department "
        "ORDER BY department DESC;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 2U);
    ASSERT_EQ(query.rows[0].size(), 2U);
    ASSERT_EQ(query.rows[1].size(), 2U);

    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Sales");
    EXPECT_EQ(query.rows[0][1].as<std::int64_t>(), 400);

    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[1][1].as<std::int64_t>(), 300);
}
TEST(DatabaseSqlExecutorTest, GroupsAllAggregateFunctionsAndHandlesNullValues) {
    DatabaseCatalog catalog;

    run_sql(
        "CREATE TABLE employees (department STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 10), "
        "('Engineering', NULL), "
        "('Engineering', 30), "
        "('Sales', NULL), "
        "('Sales', NULL);",
        catalog);

    const auto result = run_sql(
        "SELECT department, COUNT(*), COUNT(salary), SUM(salary), "
        "MIN(salary), MAX(salary), AVG(salary) "
        "FROM employees GROUP BY department;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 2U);

    // Engineering: three rows, two non-NULL salaries.
    const auto& engineering = query.rows[0];
    ASSERT_EQ(engineering.size(), 7U);
    EXPECT_EQ(engineering[0].as<std::string>(), "Engineering");
    EXPECT_EQ(engineering[1].as<std::int64_t>(), 3);
    EXPECT_EQ(engineering[2].as<std::int64_t>(), 2);
    EXPECT_EQ(engineering[3].as<std::int64_t>(), 40);
    EXPECT_EQ(engineering[4].as<std::int64_t>(), 10);
    EXPECT_EQ(engineering[5].as<std::int64_t>(), 30);
    EXPECT_DOUBLE_EQ(engineering[6].as<double>(), 20.0);

    // Sales: two rows, but every salary is NULL.
    const auto& sales = query.rows[1];
    ASSERT_EQ(sales.size(), 7U);
    EXPECT_EQ(sales[0].as<std::string>(), "Sales");
    EXPECT_EQ(sales[1].as<std::int64_t>(), 2);
    EXPECT_EQ(sales[2].as<std::int64_t>(), 0);
    EXPECT_TRUE(sales[3].is_null());
    EXPECT_TRUE(sales[4].is_null());
    EXPECT_TRUE(sales[5].is_null());
    EXPECT_TRUE(sales[6].is_null());
}
TEST(DatabaseSqlExecutorTest, GroupsByMultipleColumnsAfterFiltering) {
    DatabaseCatalog catalog;
    run_sql(
        "CREATE TABLE employees "
        "(department STRING, team STRING, salary INT64);",
        catalog);
    run_sql(
        "INSERT INTO employees VALUES "
        "('Engineering', 'Platform', 100), "
        "('Engineering', 'QA', 150), "
        "('Engineering', 'Platform', 200), "
        "('Sales', 'Platform', 90), "
        "('Sales', 'Platform', 120);",
        catalog);

    const auto result = run_sql(
        "SELECT department, team, COUNT(*), SUM(salary) "
        "FROM employees WHERE salary >= 100 "
        "GROUP BY department, team;",
        catalog);

    ASSERT_TRUE(result.query_result.has_value());
    const auto& query = *result.query_result;
    ASSERT_EQ(query.rows.size(), 3U);

    ASSERT_EQ(query.rows[0].size(), 4U);
    EXPECT_EQ(query.rows[0][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[0][1].as<std::string>(), "Platform");
    EXPECT_EQ(query.rows[0][2].as<std::int64_t>(), 2);
    EXPECT_EQ(query.rows[0][3].as<std::int64_t>(), 300);

    ASSERT_EQ(query.rows[1].size(), 4U);
    EXPECT_EQ(query.rows[1][0].as<std::string>(), "Engineering");
    EXPECT_EQ(query.rows[1][1].as<std::string>(), "QA");
    EXPECT_EQ(query.rows[1][2].as<std::int64_t>(), 1);
    EXPECT_EQ(query.rows[1][3].as<std::int64_t>(), 150);

    ASSERT_EQ(query.rows[2].size(), 4U);
    EXPECT_EQ(query.rows[2][0].as<std::string>(), "Sales");
    EXPECT_EQ(query.rows[2][1].as<std::string>(), "Platform");
    EXPECT_EQ(query.rows[2][2].as<std::int64_t>(), 1);
    EXPECT_EQ(query.rows[2][3].as<std::int64_t>(), 120);
}
}  // namespace
}  // namespace atlasdb
