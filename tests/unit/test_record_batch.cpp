#include "atlasdb/storage/record_batch.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace atlasdb {
namespace {

auto make_schema() -> Schema {
    return Schema({
        Column{"id", DataType::Int64},
        Column{"name", DataType::String},
        Column{"active", DataType::Boolean}
    });
}

auto make_row(std::int64_t id, std::string name, bool active)
    -> std::vector<Value> {
    std::vector<Value> row;
    row.emplace_back(id);
    row.emplace_back(std::move(name));
    row.emplace_back(active);
    return row;
}

TEST(RecordBatchTest, StartsEmptyWithSchemaColumns) {
    RecordBatch batch(make_schema());

    EXPECT_TRUE(batch.empty());
    EXPECT_EQ(batch.row_count(), 0U);
    EXPECT_EQ(batch.column_count(), 3U);
}

TEST(RecordBatchTest, AppendsRowsInColumnarForm) {
    RecordBatch batch(make_schema());

    batch.append_row(make_row(1, "Ada", true));
    batch.append_row(make_row(2, "Linus", false));

    EXPECT_EQ(batch.row_count(), 2U);
    EXPECT_EQ(batch.column(0).size(), 2U);
    EXPECT_EQ(batch.column(1).at(0).as<std::string>(), "Ada");
    EXPECT_EQ(batch.value_at(1, 0).as<std::int64_t>(), 2);
    EXPECT_FALSE(batch.value_at(1, 2).as<bool>());
}

TEST(RecordBatchTest, RejectsIncorrectRowWidth) {
    RecordBatch batch(make_schema());

    EXPECT_THROW(
        batch.append_row(std::vector<Value>{Value(std::int64_t{1})}),
        std::invalid_argument);

    EXPECT_EQ(batch.row_count(), 0U);
}

TEST(RecordBatchTest, RejectsInvalidValueType)
{
    RecordBatch batch(make_schema());

    std::vector<Value> invalid_row;
    invalid_row.emplace_back(std::int64_t{1});
    invalid_row.emplace_back(std::string("Ada"));
    invalid_row.emplace_back(std::string("not bool"));

    EXPECT_THROW(batch.append_row(invalid_row), std::invalid_argument);
    EXPECT_EQ(batch.row_count(), 0U);
}

TEST(RecordBatchTest, SupportsNullValues) {
    RecordBatch batch(make_schema());

    std::vector<Value> row;
    row.emplace_back(std::int64_t{1});
    row.emplace_back(Value::null());
    row.emplace_back(true);

    batch.append_row(std::move(row));

    EXPECT_TRUE(batch.value_at(0, 1).is_null());
}

TEST(RecordBatchTest, RejectsOutOfRangeRow) {
    RecordBatch batch(make_schema());

    EXPECT_THROW(batch.value_at(0, 0), std::out_of_range);
}

TEST(RecordBatchTest, RejectsOutOfRangeColumn) {
    RecordBatch batch(make_schema());

    EXPECT_THROW(batch.column(3), std::out_of_range);
}

TEST(RecordBatchTest, ClearsRows) {
    RecordBatch batch(make_schema());
    batch.append_row(make_row(1, "Ada", true));
    batch.clear();

    EXPECT_TRUE(batch.empty());
    EXPECT_EQ(batch.column(0).size(), 0U);
}

TEST(RecordBatchTest, SupportsEmptySchema) {
    RecordBatch batch(Schema(std::vector<Column>{}));

    EXPECT_EQ(batch.column_count(), 0U);
    EXPECT_NO_THROW(batch.append_row(std::vector<Value>{}));
    EXPECT_EQ(batch.row_count(), 1U);
}

}  // namespace
}  // namespace atlasdb