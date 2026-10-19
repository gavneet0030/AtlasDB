#include "atlasdb/execution/operators/filter.hpp"
#include "atlasdb/execution/operators/table_scan.hpp"
#include "atlasdb/execution/expressions/column_reference_expression.hpp"
#include "atlasdb/execution/expressions/comparison_expression.hpp"
#include "atlasdb/execution/expressions/literal_expression.hpp"
#include "atlasdb/execution/expressions/logical_expression.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace atlasdb {
namespace {

auto filter_schema() -> Schema
{
    return Schema(std::vector<Column>{
        {"id", DataType::Int64},
        {"active", DataType::Boolean}
    });
}

auto make_table() -> Table
{
    Table table(filter_schema());
    table.append_row({Value(std::int64_t{1}), Value(true)});
    table.append_row({Value(std::int64_t{2}), Value(false)});
    table.append_row({Value(std::int64_t{3}), Value(true)});
    return table;
}

TEST(FilterTest, ReturnsOnlyMatchingRows)
{
    auto table = make_table();

    Filter filter(
        std::make_unique<TableScan>(table),
        [](const std::vector<Value>& row) {
            return row[1].as<bool>();
        });

    std::vector<Value> row;

    ASSERT_TRUE(filter.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);

    ASSERT_TRUE(filter.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 3);

    EXPECT_FALSE(filter.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(FilterTest, ReturnsFalseWhenNoRowsMatch)
{
    auto table = make_table();

    Filter filter(
        std::make_unique<TableScan>(table),
        [](const std::vector<Value>&) { return false; });

    std::vector<Value> row;
    EXPECT_FALSE(filter.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(FilterTest, PreservesInputSchema)
{
    auto table = make_table();

    Filter filter(
        std::make_unique<TableScan>(table),
        [](const std::vector<Value>&) { return true; });

    EXPECT_EQ(filter.schema().column_count(), 2U);
    EXPECT_EQ(filter.schema().column_at(0).name, "id");
}

TEST(FilterTest, RejectsNullInput)
{
    EXPECT_THROW(
        Filter(nullptr, [](const std::vector<Value>&) { return true; }),
        std::invalid_argument);
}

TEST(FilterTest, RejectsEmptyPredicate)
{
    auto table = make_table();

    EXPECT_THROW(
        Filter(std::make_unique<TableScan>(table), {}),
        std::invalid_argument);
}

TEST(FilterTest, ExpressionFiltersUsingComparison)
{
    auto table = make_table();

    auto predicate = std::make_unique<ComparisonExpression>(
        ComparisonOperator::GreaterThan,
        std::make_unique<ColumnReferenceExpression>(0),
        std::make_unique<LiteralExpression>(Value(std::int64_t{1})));

    auto filter = Filter::with_expression(
        std::make_unique<TableScan>(table),
        std::move(predicate));

    std::vector<Value> row;

    ASSERT_TRUE(filter.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 2);

    ASSERT_TRUE(filter.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 3);

    EXPECT_FALSE(filter.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(FilterTest, ExpressionSupportsCompoundAndCondition)
{
    auto table = make_table();

    auto id_greater_than_one = std::make_unique<ComparisonExpression>(
        ComparisonOperator::GreaterThan,
        std::make_unique<ColumnReferenceExpression>(0),
        std::make_unique<LiteralExpression>(Value(std::int64_t{1})));

    auto active_is_true = std::make_unique<ComparisonExpression>(
        ComparisonOperator::Equal,
        std::make_unique<ColumnReferenceExpression>(1),
        std::make_unique<LiteralExpression>(Value(true)));

    auto predicate = std::make_unique<LogicalExpression>(
        LogicalOperator::And,
        std::move(id_greater_than_one),
        std::move(active_is_true));

    auto filter = Filter::with_expression(
        std::make_unique<TableScan>(table),
        std::move(predicate));

    std::vector<Value> row;

    ASSERT_TRUE(filter.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 3);
    EXPECT_TRUE(row[1].as<bool>());

    EXPECT_FALSE(filter.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(FilterTest, ExcludesRowsWhenExpressionReturnsNull)
{
    auto table = make_table();

    auto predicate = std::make_unique<LiteralExpression>(Value::null());

    auto filter = Filter::with_expression(
        std::make_unique<TableScan>(table),
        std::move(predicate));

    std::vector<Value> row;

    EXPECT_FALSE(filter.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(FilterTest, RejectsNonBooleanExpressionResult)
{
    auto table = make_table();

    auto predicate = std::make_unique<LiteralExpression>(
        Value(std::int64_t{1}));

    auto filter = Filter::with_expression(
        std::make_unique<TableScan>(table),
        std::move(predicate));

    std::vector<Value> row;

    EXPECT_THROW(filter.next(row), std::runtime_error);
}

}  // namespace
}  // namespace atlasdb
