#include "atlasdb/catalog/schema.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

TEST(SchemaTest, StoresColumns) {
    const atlasdb::Schema schema({
        {"id", atlasdb::DataType::Int64},
        {"name", atlasdb::DataType::String},
        {"active", atlasdb::DataType::Boolean}
    });

    EXPECT_EQ(schema.column_count(), 3U);
    EXPECT_EQ(schema.column_at(0).name, "id");
    EXPECT_EQ(schema.column_at(1).type, atlasdb::DataType::String);
}

TEST(SchemaTest, FindsColumnByName) {
    const atlasdb::Schema schema({
        {"id", atlasdb::DataType::Int64},
        {"score", atlasdb::DataType::Double}
    });

    ASSERT_TRUE(schema.find_column("score").has_value());
    EXPECT_EQ(*schema.find_column("score"), 1U);
    EXPECT_FALSE(schema.find_column("missing").has_value());
}

TEST(SchemaTest, RejectsDuplicateColumnNames) {
    EXPECT_THROW(
        (atlasdb::Schema({
            {"id", atlasdb::DataType::Int64},
            {"id", atlasdb::DataType::String}
        })),
        std::invalid_argument
    );
}

TEST(SchemaTest, RejectsEmptyColumnNames) {
    EXPECT_THROW(
        (atlasdb::Schema({
            {"", atlasdb::DataType::Int64}
        })),
        std::invalid_argument
    );
}

TEST(SchemaTest, RejectsOutOfRangeColumnIndex) {
    const atlasdb::Schema schema({
        {"id", atlasdb::DataType::Int64}
    });

    EXPECT_THROW((void)schema.column_at(1), std::out_of_range);
}

TEST(SchemaTest, AllowsEmptySchema) {
    const atlasdb::Schema schema({});

    EXPECT_EQ(schema.column_count(), 0U);
    EXPECT_FALSE(schema.find_column("id").has_value());
}
