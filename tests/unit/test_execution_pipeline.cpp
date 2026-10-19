#include "atlasdb/execution/operators/filter.hpp"
#include "atlasdb/execution/operators/projection.hpp"
#include "atlasdb/execution/operators/sort.hpp"
#include "atlasdb/execution/operators/table_scan.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace atlasdb {
namespace {

auto make_pipeline_table() -> Table
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
    table.append_row({
        Value(std::int64_t{3}), Value("Rahul"), Value(true)
    });

    return table;
}

TEST(ExecutionPipelineTest, FiltersThenProjectsColumns)
{
    auto table = make_pipeline_table();

    auto scan = std::make_unique<TableScan>(table);

    auto filter = std::make_unique<Filter>(
        std::move(scan),
        [](const std::vector<Value>& row) {
            return !row[2].is_null() && row[2].as<bool>();
        });

    Projection projection(std::move(filter), {1});

    ASSERT_EQ(projection.schema().column_count(), 1U);
    EXPECT_EQ(projection.schema().column_at(0).name, "name");

    std::vector<Value> row;

    ASSERT_TRUE(projection.next(row));
    ASSERT_EQ(row.size(), 1U);
    EXPECT_EQ(row[0].as<std::string>(), "Aman");

    ASSERT_TRUE(projection.next(row));
    ASSERT_EQ(row.size(), 1U);
    EXPECT_EQ(row[0].as<std::string>(), "Rahul");

    EXPECT_FALSE(projection.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(ExecutionPipelineTest, HandlesNoMatchingRows)
{
    auto table = make_pipeline_table();

    auto filter = std::make_unique<Filter>(
        std::make_unique<TableScan>(table),
        [](const std::vector<Value>& row) {
            return !row[2].is_null() && !row[2].as<bool>();
        });

    Projection projection(std::move(filter), {1});

    std::vector<Value> row;

    ASSERT_TRUE(projection.next(row));
    EXPECT_EQ(row[0].as<std::string>(), "Priya");

    EXPECT_FALSE(projection.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(ExecutionPipelineTest, PreservesNullValuesInProjectedColumns)
{
    Schema schema(std::vector<Column>{
        {"id", DataType::Int64},
        {"name", DataType::String},
        {"active", DataType::Boolean}
    });

    Table table(std::move(schema));
    table.append_row({
        Value(std::int64_t{1}), Value::null(), Value(true)
    });

    auto filter = std::make_unique<Filter>(
        std::make_unique<TableScan>(table),
        [](const std::vector<Value>& row) {
            return !row[2].is_null() && row[2].as<bool>();
        });

    Projection projection(std::move(filter), {1});

    std::vector<Value> row;
    ASSERT_TRUE(projection.next(row));
    ASSERT_EQ(row.size(), 1U);
    EXPECT_TRUE(row[0].is_null());
    EXPECT_FALSE(projection.next(row));
}


TEST(ExecutionPipelineTest, FiltersProjectsThenSorts)
{
    auto table = make_pipeline_table();

    auto filter = std::make_unique<Filter>(
        std::make_unique<TableScan>(table),
        [](const std::vector<Value>& row) {
            return !row[2].is_null() && row[2].as<bool>();
        });

    auto projection = std::make_unique<Projection>(
        std::move(filter),
        std::vector<std::size_t>{0, 1});

    Sort sort(
        std::move(projection),
        {{0, SortDirection::Descending, NullPlacement::Last}});

    ASSERT_EQ(sort.schema().column_count(), 2U);
    EXPECT_EQ(sort.schema().column_at(0).name, "id");
    EXPECT_EQ(sort.schema().column_at(1).name, "name");

    std::vector<Value> row;

    ASSERT_TRUE(sort.next(row));
    ASSERT_EQ(row.size(), 2U);
    EXPECT_EQ(row[0].as<std::int64_t>(), 3);
    EXPECT_EQ(row[1].as<std::string>(), "Rahul");

    ASSERT_TRUE(sort.next(row));
    ASSERT_EQ(row.size(), 2U);
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);
    EXPECT_EQ(row[1].as<std::string>(), "Aman");

    EXPECT_FALSE(sort.next(row));
}
}  // namespace
}  // namespace atlasdb
