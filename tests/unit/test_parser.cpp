#include "atlasdb/parser/parser.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace atlasdb {
namespace {

TEST(ParserTest, ParsesBasicSelect) {
    Parser parser("SELECT name, salary FROM employees;");
    auto query = parser.parse();

    EXPECT_FALSE(query.select_all);
    ASSERT_EQ(query.columns.size(), 2U);
    EXPECT_EQ(query.columns[0], "name");
    EXPECT_EQ(query.columns[1], "salary");
    EXPECT_EQ(query.table, "employees");
    EXPECT_EQ(query.where, nullptr);
    EXPECT_TRUE(query.order_by.empty());
}

TEST(ParserTest, ParsesCountStar) {
    Parser parser("SELECT COUNT(*) FROM employees");
    auto query = parser.parse();

    EXPECT_FALSE(query.select_all);
    EXPECT_TRUE(query.columns.empty());
    ASSERT_EQ(query.aggregates.size(), 1U);
    EXPECT_EQ(query.aggregates[0].function, "COUNT");
    EXPECT_TRUE(query.aggregates[0].star_argument);
    EXPECT_TRUE(query.aggregates[0].column.empty());
    EXPECT_EQ(query.table, "employees");
}

TEST(ParserTest, ParsesMultipleAggregateFunctions) {
    Parser parser(
        "SELECT SUM(salary), AVG(salary), MIN(salary), MAX(salary), "
        "COUNT(salary) FROM employees");
    auto query = parser.parse();

    ASSERT_EQ(query.aggregates.size(), 5U);
    EXPECT_EQ(query.aggregates[0].function, "SUM");
    EXPECT_EQ(query.aggregates[1].function, "AVG");
    EXPECT_EQ(query.aggregates[2].function, "MIN");
    EXPECT_EQ(query.aggregates[3].function, "MAX");
    EXPECT_EQ(query.aggregates[4].function, "COUNT");

    for (const auto& aggregate : query.aggregates) {
        EXPECT_EQ(aggregate.column, "salary");
        EXPECT_FALSE(aggregate.star_argument);
    }
}

TEST(ParserTest, ParsesAggregateWithWhereClause) {
    Parser parser(
        "SELECT SUM(salary) FROM employees WHERE salary > 50000");
    auto query = parser.parse();

    ASSERT_EQ(query.aggregates.size(), 1U);
    ASSERT_NE(query.where, nullptr);
    EXPECT_EQ(query.where->kind, ExpressionKind::Greater);
    EXPECT_EQ(query.where->left->value, "salary");
}

TEST(ParserTest, RejectsMixingAggregatesAndOrdinaryColumns) {
    Parser parser("SELECT name, COUNT(*) FROM employees");

    EXPECT_THROW(parser.parse(), std::exception);
}

TEST(ParserTest, ParsesSelectAll) {
    Parser parser("SELECT * FROM employees");
    auto query = parser.parse();

    EXPECT_TRUE(query.select_all);
    EXPECT_TRUE(query.columns.empty());
    EXPECT_EQ(query.table, "employees");
}

TEST(ParserTest, ParsesComparisonAndLiteral) {
    Parser parser("SELECT name FROM employees WHERE salary > 50000");
    auto query = parser.parse();

    ASSERT_NE(query.where, nullptr);
    EXPECT_EQ(query.where->kind, ExpressionKind::Greater);
    ASSERT_NE(query.where->left, nullptr);
    EXPECT_EQ(query.where->left->kind, ExpressionKind::Column);
    EXPECT_EQ(query.where->left->value, "salary");
    ASSERT_NE(query.where->right, nullptr);
    EXPECT_EQ(query.where->right->kind, ExpressionKind::Literal);
    EXPECT_EQ(query.where->right->value, "50000");
    EXPECT_EQ(query.where->right->literal_kind, TokenKind::IntegerLiteral);
}

TEST(ParserTest, AndHasHigherPrecedenceThanOr) {
    Parser parser(
        "SELECT * FROM employees "
        "WHERE age > 18 OR salary > 50000 AND active = TRUE");
    auto query = parser.parse();

    ASSERT_NE(query.where, nullptr);
    EXPECT_EQ(query.where->kind, ExpressionKind::Or);
    ASSERT_NE(query.where->right, nullptr);
    EXPECT_EQ(query.where->right->kind, ExpressionKind::And);
}

TEST(ParserTest, ParsesParenthesizedConditionsAndNot) {
    Parser parser(
        "SELECT * FROM employees "
        "WHERE NOT (age < 18 OR active = FALSE)");
    auto query = parser.parse();

    ASSERT_NE(query.where, nullptr);
    EXPECT_EQ(query.where->kind, ExpressionKind::Not);
    ASSERT_NE(query.where->left, nullptr);
    EXPECT_EQ(query.where->left->kind, ExpressionKind::Or);
}

TEST(ParserTest, ParsesAggregateExpressionInOrderBy) {
    Parser parser(
        "SELECT department, SUM(salary) FROM employees "
        "GROUP BY department ORDER BY SUM(salary) DESC");
    const auto query = parser.parse();

    ASSERT_EQ(query.order_by.size(), 1U);
    EXPECT_TRUE(query.order_by[0].is_aggregate);
    EXPECT_EQ(query.order_by[0].aggregate_function, "SUM");
    EXPECT_EQ(query.order_by[0].aggregate_column, "salary");
    EXPECT_FALSE(query.order_by[0].ascending);
}

TEST(ParserTest, ParsesOrderByDirections) {
    Parser parser(
        "SELECT name FROM employees ORDER BY department ASC, salary DESC");
    auto query = parser.parse();

    ASSERT_EQ(query.order_by.size(), 2U);
    EXPECT_EQ(query.order_by[0].column, "department");
    EXPECT_TRUE(query.order_by[0].ascending);
    EXPECT_EQ(query.order_by[1].column, "salary");
    EXPECT_FALSE(query.order_by[1].ascending);
}

TEST(ParserTest, ParsesStringBooleanAndNullLiterals) {
    Parser parser(
        "SELECT name FROM employees "
        "WHERE name = 'O''Brien' AND active = TRUE AND manager = NULL");
    auto query = parser.parse();

    ASSERT_NE(query.where, nullptr);
    EXPECT_EQ(query.where->kind, ExpressionKind::And);
    ASSERT_NE(query.where->left, nullptr);
    EXPECT_EQ(query.where->left->kind, ExpressionKind::And);
    ASSERT_NE(query.where->right, nullptr);
    EXPECT_EQ(query.where->right->kind, ExpressionKind::Equal);
}

TEST(ParserTest, ParsesGroupByColumns) {
    Parser parser(
        "SELECT department FROM employees "
        "WHERE active = TRUE GROUP BY department, team "
        "ORDER BY department DESC");
    auto query = parser.parse();

    ASSERT_EQ(query.group_by.size(), 2U);
    EXPECT_EQ(query.group_by[0], "department");
    EXPECT_EQ(query.group_by[1], "team");
    ASSERT_NE(query.where, nullptr);
    ASSERT_EQ(query.order_by.size(), 1U);
    EXPECT_EQ(query.order_by[0].column, "department");
    EXPECT_FALSE(query.order_by[0].ascending);
}

TEST(ParserTest, ParsesGroupByWithoutWhereOrOrderBy) {
    Parser parser("SELECT department FROM employees GROUP BY department");
    auto query = parser.parse();

    ASSERT_EQ(query.group_by.size(), 1U);
    EXPECT_EQ(query.group_by[0], "department");
    EXPECT_EQ(query.where, nullptr);
    EXPECT_TRUE(query.order_by.empty());
}

TEST(ParserTest, RejectsMissingGroupByColumn) {
    Parser parser("SELECT department FROM employees GROUP BY");

    EXPECT_THROW(static_cast<void>(parser.parse()), std::runtime_error);
}

TEST(ParserTest, RejectsMissingTableName) {
    Parser parser("SELECT name FROM");

    EXPECT_THROW(static_cast<void>(parser.parse()), std::runtime_error);
}

TEST(ParserTest, RejectsUnexpectedTrailingTokens) {
    Parser parser("SELECT name FROM employees extra");

    EXPECT_THROW(static_cast<void>(parser.parse()), std::runtime_error);
}

TEST(ParserTest, RejectsMissingClosingParenthesis) {
    Parser parser(
        "SELECT * FROM employees WHERE (salary > 50000");

    EXPECT_THROW(static_cast<void>(parser.parse()), std::runtime_error);
}

TEST(ParserTest, RejectsEmptySelectList) {
    Parser parser("SELECT FROM employees");

    EXPECT_THROW(static_cast<void>(parser.parse()), std::runtime_error);
}

TEST(ParserTest, ParsesCreateTableWithAllSupportedTypes) {
    Parser parser(
        "CREATE TABLE employees "
        "(id INT64, salary DOUBLE, active BOOLEAN, name STRING);");

    auto statement = parser.parse_statement();

    EXPECT_EQ(statement.kind, StatementKind::CreateTable);
    EXPECT_EQ(statement.create_table.table_name, "employees");
    ASSERT_EQ(statement.create_table.columns.size(), 4U);
    EXPECT_EQ(statement.create_table.columns[0].name, "id");
    EXPECT_EQ(statement.create_table.columns[0].type, DataType::Int64);
    EXPECT_EQ(statement.create_table.columns[1].type, DataType::Double);
    EXPECT_EQ(statement.create_table.columns[2].type, DataType::Boolean);
    EXPECT_EQ(statement.create_table.columns[3].type, DataType::String);
}

TEST(ParserTest, ParsesInsertWithMultipleRowsAndLiterals) {
    Parser parser(
        "INSERT INTO employees (id, name, active, salary) "
        "VALUES (1, 'Aman', TRUE, 42000.5), "
        "(2, 'O''Brien', FALSE, NULL);");

    auto statement = parser.parse_statement();

    EXPECT_EQ(statement.kind, StatementKind::Insert);
    EXPECT_EQ(statement.insert.table_name, "employees");
    ASSERT_EQ(statement.insert.columns.size(), 4U);
    EXPECT_EQ(statement.insert.columns[1], "name");
    ASSERT_EQ(statement.insert.rows.size(), 2U);
    ASSERT_EQ(statement.insert.rows[0].size(), 4U);
    EXPECT_EQ(statement.insert.rows[0][0].kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(statement.insert.rows[0][1].value, "Aman");
    EXPECT_EQ(statement.insert.rows[0][2].kind, TokenKind::True);
    EXPECT_EQ(statement.insert.rows[0][3].kind, TokenKind::FloatingLiteral);
    EXPECT_EQ(statement.insert.rows[1][1].value, "O'Brien");
    EXPECT_EQ(statement.insert.rows[1][2].kind, TokenKind::False);
    EXPECT_EQ(statement.insert.rows[1][3].kind, TokenKind::Null);
}

TEST(ParserTest, ParsesInsertWithoutExplicitColumnList) {
    Parser parser("INSERT INTO metrics VALUES (10, 3.5);");

    auto statement = parser.parse_statement();

    EXPECT_EQ(statement.kind, StatementKind::Insert);
    EXPECT_TRUE(statement.insert.columns.empty());
    ASSERT_EQ(statement.insert.rows.size(), 1U);
    ASSERT_EQ(statement.insert.rows[0].size(), 2U);
}

TEST(ParserTest, RejectsCreateTableWithMissingColumnType) {
    Parser parser("CREATE TABLE employees (id);");

    EXPECT_THROW(static_cast<void>(parser.parse_statement()),
                 std::runtime_error);
}

TEST(ParserTest, RejectsInsertWithMissingValues) {
    Parser parser("INSERT INTO employees VALUES ();");

    EXPECT_THROW(static_cast<void>(parser.parse_statement()),
                 std::runtime_error);
}

TEST(ParserTest, ParsesInnerJoinAndWhereCondition) {
    Parser parser(
        "SELECT users.name, orders.total FROM users "
        "INNER JOIN orders ON users.id = orders.user_id "
        "WHERE orders.total >= 10;");
    const auto query = parser.parse();

    EXPECT_EQ(query.table, "users");
    EXPECT_EQ(query.join_table, "orders");
    ASSERT_NE(query.join_condition, nullptr);
    EXPECT_EQ(query.join_condition->kind, ExpressionKind::Equal);
    ASSERT_NE(query.join_condition->left, nullptr);
    EXPECT_EQ(query.join_condition->left->value, "users.id");
    ASSERT_NE(query.join_condition->right, nullptr);
    EXPECT_EQ(query.join_condition->right->value, "orders.user_id");
    ASSERT_NE(query.where, nullptr);
    EXPECT_EQ(query.where->kind, ExpressionKind::GreaterEqual);
}

TEST(ParserTest, RejectsInnerJoinWithoutOn) {
    Parser parser("SELECT * FROM users INNER JOIN orders;");
    EXPECT_THROW(static_cast<void>(parser.parse()), std::runtime_error);
}

TEST(ParserTest, ParsesSelectThroughStatementApi) {
    Parser parser("SELECT name FROM employees;");
    auto statement = parser.parse_statement();

    EXPECT_EQ(statement.kind, StatementKind::Select);
    EXPECT_EQ(statement.select.table, "employees");
    ASSERT_EQ(statement.select.columns.size(), 1U);
    EXPECT_EQ(statement.select.columns[0], "name");
}

}  // namespace
}  // namespace atlasdb
