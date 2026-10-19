#include "atlasdb/execution/operators/table_scan.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace atlasdb {
namespace {

auto make_schema() -> Schema
{
    return Schema(std::vector<Column>{
        {"id", DataType::Int64},
        {"name", DataType::String}
    });
}

TEST(TableScanTest, ReadsAllRowsInOrder)
{
    Table table(make_schema());
    table.append_row({Value(std::int64_t{1}), Value("Ada")});
    table.append_row({Value(std::int64_t{2}), Value("Linus")});

    TableScan scan(table);
    std::vector<Value> row;

    ASSERT_TRUE(scan.next(row));
    ASSERT_EQ(row.size(), 2U);
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);
    EXPECT_EQ(row[1].as<std::string>(), "Ada");

    ASSERT_TRUE(scan.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 2);
    EXPECT_EQ(row[1].as<std::string>(), "Linus");

    EXPECT_FALSE(scan.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(TableScanTest, ReturnsFalseForEmptyTable)
{
    Table table(make_schema());
    TableScan scan(table);
    std::vector<Value> row;

    EXPECT_FALSE(scan.next(row));
}

TEST(TableScanTest, ExposesTableSchema)
{
    Table table(make_schema());
    TableScan scan(table);

    EXPECT_EQ(scan.schema().column_count(), 2U);
    EXPECT_EQ(scan.schema().column_at(0).name, "id");
    EXPECT_EQ(scan.schema().column_at(1).name, "name");
}

TEST(TableScanTest, SupportsNullValues)
{
    Table table(make_schema());
    table.append_row({
        Value(std::int64_t{1}),
        Value::null()
    });

    TableScan scan(table);
    std::vector<Value> row;

    ASSERT_TRUE(scan.next(row));
    EXPECT_TRUE(row[1].is_null());
}

}  // namespace
}  // namespace atlasdb
