#include "atlasdb/execution/operators/projection.hpp"
#include "atlasdb/execution/operators/table_scan.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace atlasdb {
namespace {

auto make_projection_table() -> Table
{
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

    return table;
}

TEST(ProjectionTest, SelectsRequestedColumns)
{
    auto table = make_projection_table();

    Projection projection(
        std::make_unique<TableScan>(table), {1, 2});

    EXPECT_EQ(projection.schema().column_count(), 2U);
    EXPECT_EQ(projection.schema().column_at(0).name, "name");
    EXPECT_EQ(projection.schema().column_at(1).name, "active");

    std::vector<Value> row;
    ASSERT_TRUE(projection.next(row));
    ASSERT_EQ(row.size(), 2U);
    EXPECT_EQ(row[0].as<std::string>(), "Aman");
    EXPECT_TRUE(row[1].as<bool>());

    ASSERT_TRUE(projection.next(row));
    EXPECT_EQ(row[0].as<std::string>(), "Priya");
    EXPECT_FALSE(row[1].as<bool>());

    EXPECT_FALSE(projection.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(ProjectionTest, PreservesRequestedColumnOrder)
{
    auto table = make_projection_table();

    Projection projection(
        std::make_unique<TableScan>(table), {2, 0});

    EXPECT_EQ(projection.schema().column_at(0).name, "active");
    EXPECT_EQ(projection.schema().column_at(1).name, "id");

    std::vector<Value> row;
    ASSERT_TRUE(projection.next(row));
    EXPECT_TRUE(row[0].as<bool>());
    EXPECT_EQ(row[1].as<std::int64_t>(), 1);
}

TEST(ProjectionTest, SupportsEmptyColumnSelection)
{
    auto table = make_projection_table();

    Projection projection(
        std::make_unique<TableScan>(table), {});

    EXPECT_EQ(projection.schema().column_count(), 0U);

    std::vector<Value> row;
    ASSERT_TRUE(projection.next(row));
    EXPECT_TRUE(row.empty());

    ASSERT_TRUE(projection.next(row));
    EXPECT_TRUE(row.empty());

    EXPECT_FALSE(projection.next(row));
}

TEST(ProjectionTest, RejectsOutOfRangeColumnIndex)
{
    auto table = make_projection_table();

    EXPECT_THROW(
        Projection(std::make_unique<TableScan>(table), {3}),
        std::out_of_range);
}

TEST(ProjectionTest, ReturnsFalseForEmptyInput)
{
    Schema schema(std::vector<Column>{
        {"id", DataType::Int64}
    });
    Table table(std::move(schema));

    Projection projection(
        std::make_unique<TableScan>(table), {0});

    std::vector<Value> row;
    EXPECT_FALSE(projection.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(ProjectionTest, RejectsNullInput)
{
    EXPECT_THROW(
        Projection(nullptr, {0}),
        std::invalid_argument);
}

}  // namespace
}  // namespace atlasdb
