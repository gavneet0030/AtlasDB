#include "atlasdb/storage/column_vector.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace atlasdb {
namespace {

TEST(ColumnVectorTest, StartsEmpty) {
    ColumnVector column(DataType::Int64);

    EXPECT_TRUE(column.empty());
    EXPECT_EQ(column.size(), 0U);
    EXPECT_EQ(column.type(), DataType::Int64);
}

TEST(ColumnVectorTest, AppendsIntegers) {
    ColumnVector column(DataType::Int64);

    column.append(Value(std::int64_t{10}));
    column.append(Value(std::int64_t{20}));

    ASSERT_EQ(column.size(), 2U);
    EXPECT_EQ(column.at(0).as<std::int64_t>(), 10);
    EXPECT_EQ(column.at(1).as<std::int64_t>(), 20);
}

TEST(ColumnVectorTest, AppendsStrings) {
    ColumnVector column(DataType::String);
    column.append(Value(std::string("AtlasDB")));

    ASSERT_EQ(column.size(), 1U);
    EXPECT_EQ(column.at(0).as<std::string>(), "AtlasDB");
}

TEST(ColumnVectorTest, AllowsNullValues) {
    ColumnVector column(DataType::Double);
    column.append(Value::null());

    ASSERT_EQ(column.size(), 1U);
    EXPECT_TRUE(column.at(0).is_null());
}

TEST(ColumnVectorTest, RejectsIncompatibleValues) {
    ColumnVector column(DataType::Int64);

    EXPECT_THROW(
        column.append(Value(std::string("wrong type"))),
        std::invalid_argument);

    EXPECT_TRUE(column.empty());
}

TEST(ColumnVectorTest, ChecksBounds) {
    ColumnVector column(DataType::Boolean);

    EXPECT_THROW(column.at(0), std::out_of_range);
}

TEST(ColumnVectorTest, ClearsValues) {
    ColumnVector column(DataType::Int64);
    column.append(Value(std::int64_t{1}));
    column.clear();

    EXPECT_TRUE(column.empty());
}

TEST(ColumnVectorTest, ReservesCapacityWithoutAddingRows) {
    ColumnVector column(DataType::Int64);
    column.reserve(100);

    EXPECT_EQ(column.size(), 0U);
}

}  // namespace
}  // namespace atlasdb