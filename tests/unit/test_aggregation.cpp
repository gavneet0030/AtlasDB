#include "atlasdb/execution/operators/aggregation.hpp"
#include "atlasdb/execution/operators/table_scan.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace atlasdb {
namespace {

auto make_table() -> Table
{
    Schema schema(std::vector<Column>{
        {"amount", DataType::Int64},
        {"price", DataType::Double}
    });

    Table table(std::move(schema));
    table.append_row({Value(10), Value(1.5)});
    table.append_row({Value(20), Value(2.5)});
    table.append_row({Value::null(), Value(4.0)});
    return table;
}

TEST(AggregationTest, CountsAllRowsAndNonNullValues)
{
    auto table = make_table();
    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        {{AggregateFunction::Count, std::nullopt},
         {AggregateFunction::Count, 0}});

    std::vector<Value> row;
    ASSERT_TRUE(aggregation.next(row));
    ASSERT_EQ(row.size(), 2U);
    EXPECT_EQ(row[0].as<std::int64_t>(), 3);
    EXPECT_EQ(row[1].as<std::int64_t>(), 2);
    EXPECT_FALSE(aggregation.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(AggregationTest, SumsIntegerColumn)
{
    auto table = make_table();
    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        {{AggregateFunction::Sum, 0}});

    std::vector<Value> row;
    ASSERT_TRUE(aggregation.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 30);
}

TEST(AggregationTest, SumsDoubleColumn)
{
    auto table = make_table();
    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        {{AggregateFunction::Sum, 1}});

    std::vector<Value> row;
    ASSERT_TRUE(aggregation.next(row));
    EXPECT_DOUBLE_EQ(row[0].as<double>(), 8.0);
}

TEST(AggregationTest, ComputesAverageIgnoringNulls)
{
    auto table = make_table();
    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        {{AggregateFunction::Avg, 0}});

    EXPECT_EQ(aggregation.schema().column_at(0).type, DataType::Double);

    std::vector<Value> row;
    ASSERT_TRUE(aggregation.next(row));
    EXPECT_DOUBLE_EQ(row[0].as<double>(), 15.0);
}

TEST(AggregationTest, FindsMinimumAndMaximum)
{
    auto table = make_table();
    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        {{AggregateFunction::Min, 0},
         {AggregateFunction::Max, 0}});

    std::vector<Value> row;
    ASSERT_TRUE(aggregation.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 10);
    EXPECT_EQ(row[1].as<std::int64_t>(), 20);
}

TEST(AggregationTest, ReturnsZeroCountsAndNullForEmptyInput)
{
    Schema schema(std::vector<Column>{
        {"amount", DataType::Int64}
    });
    Table table(std::move(schema));

    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        {{AggregateFunction::Count, std::nullopt},
         {AggregateFunction::Sum, 0},
         {AggregateFunction::Avg, 0},
         {AggregateFunction::Min, 0},
         {AggregateFunction::Max, 0}});

    std::vector<Value> row;
    ASSERT_TRUE(aggregation.next(row));
    ASSERT_EQ(row.size(), 5U);
    EXPECT_EQ(row[0].as<std::int64_t>(), 0);
    EXPECT_TRUE(row[1].is_null());
    EXPECT_TRUE(row[2].is_null());
    EXPECT_TRUE(row[3].is_null());
    EXPECT_TRUE(row[4].is_null());
    EXPECT_FALSE(aggregation.next(row));
}

TEST(AggregationTest, RejectsNullInput)
{
    EXPECT_THROW(
        Aggregation(nullptr, {{AggregateFunction::Count, std::nullopt}}),
        std::invalid_argument);
}

TEST(AggregationTest, RejectsEmptyAggregateList)
{
    auto table = make_table();
    EXPECT_THROW(
        Aggregation(std::make_unique<TableScan>(table), {}),
        std::invalid_argument);
}

TEST(AggregationTest, RejectsMissingColumnForSum)
{
    auto table = make_table();
    EXPECT_THROW(
        Aggregation(
            std::make_unique<TableScan>(table),
            {{AggregateFunction::Sum, std::nullopt}}),
        std::invalid_argument);
}

TEST(AggregationTest, RejectsStringOrBooleanAggregateColumns)
{
    Schema schema(std::vector<Column>{
        {"name", DataType::String}
    });
    Table table(std::move(schema));

    EXPECT_THROW(
        Aggregation(
            std::make_unique<TableScan>(table),
            {{AggregateFunction::Sum, 0}}),
        std::invalid_argument);
}

TEST(AggregationTest, RejectsOutOfRangeColumn)
{
    auto table = make_table();
    EXPECT_THROW(
        Aggregation(
            std::make_unique<TableScan>(table),
            {{AggregateFunction::Count, 9}}),
        std::out_of_range);
}

TEST(AggregationTest, GroupsRowsAndComputesCountAndSum)
{
    Schema schema(std::vector<Column>{
        {"department", DataType::String},
        {"salary", DataType::Int64}
    });
    Table table(std::move(schema));

    table.append_row({Value("Engineering"), Value(100)});
    table.append_row({Value("Sales"), Value(50)});
    table.append_row({Value("Engineering"), Value(200)});

    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        std::vector<std::size_t>{0},
        {{AggregateFunction::Count, std::nullopt},
         {AggregateFunction::Sum, 1}});

    std::vector<Value> row;

    ASSERT_TRUE(aggregation.next(row));
    ASSERT_EQ(row.size(), 3U);
    EXPECT_EQ(row[0].as<std::string>(), "Engineering");
    EXPECT_EQ(row[1].as<std::int64_t>(), 2);
    EXPECT_EQ(row[2].as<std::int64_t>(), 300);

    ASSERT_TRUE(aggregation.next(row));
    ASSERT_EQ(row.size(), 3U);
    EXPECT_EQ(row[0].as<std::string>(), "Sales");
    EXPECT_EQ(row[1].as<std::int64_t>(), 1);
    EXPECT_EQ(row[2].as<std::int64_t>(), 50);

    EXPECT_FALSE(aggregation.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(AggregationTest, ReturnsNoGroupsForEmptyGroupedInput)
{
    Schema schema(std::vector<Column>{
        {"department", DataType::String},
        {"salary", DataType::Int64}
    });
    Table table(std::move(schema));

    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        std::vector<std::size_t>{0},
        {{AggregateFunction::Count, std::nullopt}});

    std::vector<Value> row;
    EXPECT_FALSE(aggregation.next(row));
    EXPECT_TRUE(row.empty());
}
TEST(AggregationTest, GroupsNullKeysTogether)
{
    Schema schema(std::vector<Column>{
        {"department", DataType::String},
        {"salary", DataType::Int64}
    });
    Table table(std::move(schema));

    table.append_row({Value::null(), Value(10)});
    table.append_row({Value("Engineering"), Value(20)});
    table.append_row({Value::null(), Value(30)});

    Aggregation aggregation(
        std::make_unique<TableScan>(table),
        std::vector<std::size_t>{0},
        {{AggregateFunction::Count, std::nullopt},
         {AggregateFunction::Sum, 1}});

    std::vector<Value> row;

    ASSERT_TRUE(aggregation.next(row));
    ASSERT_EQ(row.size(), 3U);
    EXPECT_TRUE(row[0].is_null());
    EXPECT_EQ(row[1].as<std::int64_t>(), 2);
    EXPECT_EQ(row[2].as<std::int64_t>(), 40);

    ASSERT_TRUE(aggregation.next(row));
    EXPECT_EQ(row[0].as<std::string>(), "Engineering");
    EXPECT_EQ(row[1].as<std::int64_t>(), 1);
    EXPECT_EQ(row[2].as<std::int64_t>(), 20);

    EXPECT_FALSE(aggregation.next(row));
}
}  // namespace
}  // namespace atlasdb
