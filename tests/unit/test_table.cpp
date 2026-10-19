#include "atlasdb/storage/table.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace atlasdb {
namespace {

auto schema_a() -> Schema {
    return Schema({
        Column{"id", DataType::Int64},
        Column{"label", DataType::String}
    });
}

auto row(std::int64_t id, std::string label) -> std::vector<Value> {
    std::vector<Value> values;
    values.emplace_back(id);
    values.emplace_back(std::move(label));
    return values;
}

TEST(TableTest, StartsEmpty) {
    Table table(schema_a());

    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.row_count(), 0U);
    EXPECT_EQ(table.batch_count(), 0U);
}

TEST(TableTest, AppendsRows) {
    Table table(schema_a());

    table.append_row(row(1, "one"));
    table.append_row(row(2, "two"));

    EXPECT_EQ(table.row_count(), 2U);
    EXPECT_EQ(table.batch_count(), 1U);
    EXPECT_EQ(table.value_at(1, 1).as<std::string>(), "two");
}

TEST(TableTest, AppendsExplicitRecordBatch) {
    Table table(schema_a());
    RecordBatch batch(schema_a());

    batch.append_row(row(1, "one"));
    batch.append_row(row(2, "two"));

    table.append_batch(std::move(batch));

    EXPECT_EQ(table.row_count(), 2U);
    EXPECT_EQ(table.batch_count(), 1U);
    EXPECT_EQ(table.value_at(0, 0).as<std::int64_t>(), 1);
    EXPECT_EQ(table.value_at(1, 1).as<std::string>(), "two");
}

TEST(TableTest, RejectsMismatchedSchema) {
    Table table(schema_a());
    RecordBatch incompatible(Schema({
        Column{"other", DataType::Int64}
    }));

    EXPECT_THROW(
        table.append_batch(std::move(incompatible)),
        std::invalid_argument);

    EXPECT_EQ(table.row_count(), 0U);
}

TEST(TableTest, ReadsAcrossMultipleBatches) {
    Table table(schema_a());

    RecordBatch first(schema_a());
    first.append_row(row(10, "first"));
    table.append_batch(std::move(first));

    RecordBatch second(schema_a());
    second.append_row(row(20, "second"));
    table.append_batch(std::move(second));

    EXPECT_EQ(table.row_count(), 2U);
    EXPECT_EQ(table.batch_count(), 2U);
    EXPECT_EQ(table.value_at(1, 0).as<std::int64_t>(), 20);
    EXPECT_EQ(table.value_at(1, 1).as<std::string>(), "second");
}

TEST(TableTest, RejectsOutOfRangeAccess) {
    Table table(schema_a());

    EXPECT_THROW(table.value_at(0, 0), std::out_of_range);
}

TEST(TableTest, ClearsAllBatches) {
    Table table(schema_a());
    table.append_row(row(1, "one"));
    table.clear();

    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.batch_count(), 0U);
}

TEST(TableTest, RejectsRowsWithInvalidWidth) {
    Table table(schema_a());

    EXPECT_THROW(
        table.append_row(std::vector<Value>{Value(std::int64_t{1})}),
        std::invalid_argument);

    EXPECT_EQ(table.row_count(), 0U);
}

}  // namespace
}  // namespace atlasdb