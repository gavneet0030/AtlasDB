#include "atlasdb/types/value.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

TEST(ValueTest, StoresInteger) {
    const atlasdb::Value value{std::int64_t{42}};

    ASSERT_TRUE(value.data_type().has_value());
    EXPECT_EQ(*value.data_type(), atlasdb::DataType::Int64);
    EXPECT_EQ(value.as<std::int64_t>(), 42);
    EXPECT_EQ(value.to_string(), "42");
}

TEST(ValueTest, StoresDouble) {
    const atlasdb::Value value{3.5};

    ASSERT_TRUE(value.data_type().has_value());
    EXPECT_EQ(*value.data_type(), atlasdb::DataType::Double);
    EXPECT_EQ(value.as<double>(), 3.5);
}

TEST(ValueTest, StoresBoolean) {
    const atlasdb::Value value{true};

    ASSERT_TRUE(value.data_type().has_value());
    EXPECT_EQ(*value.data_type(), atlasdb::DataType::Boolean);
    EXPECT_EQ(value.to_string(), "TRUE");
}

TEST(ValueTest, StoresString) {
    const atlasdb::Value value{std::string{"AtlasDB"}};

    ASSERT_TRUE(value.data_type().has_value());
    EXPECT_EQ(*value.data_type(), atlasdb::DataType::String);
    EXPECT_EQ(value.as<std::string>(), "AtlasDB");
}

TEST(ValueTest, NullHasNoIntrinsicType) {
    const auto value = atlasdb::Value::null();

    EXPECT_TRUE(value.is_null());
    EXPECT_FALSE(value.data_type().has_value());
    EXPECT_EQ(value.to_string(), "NULL");
}

TEST(ValueTest, NullIsCompatibleWithAnyColumnType) {
    const auto value = atlasdb::Value::null();

    EXPECT_TRUE(value.is_compatible_with(atlasdb::DataType::Int64));
    EXPECT_TRUE(value.is_compatible_with(atlasdb::DataType::String));
}

TEST(ValueTest, RejectsIncompatibleType) {
    const atlasdb::Value value{42};

    EXPECT_FALSE(value.is_compatible_with(atlasdb::DataType::String));
    EXPECT_TRUE(value.is_compatible_with(atlasdb::DataType::Int64));
}

TEST(DataTypeTest, ConvertsTypesToNames) {
    EXPECT_EQ(atlasdb::to_string(atlasdb::DataType::Int64), "INT64");
    EXPECT_EQ(atlasdb::to_string(atlasdb::DataType::Double), "DOUBLE");
    EXPECT_EQ(atlasdb::to_string(atlasdb::DataType::Boolean), "BOOLEAN");
    EXPECT_EQ(atlasdb::to_string(atlasdb::DataType::String), "STRING");
}
