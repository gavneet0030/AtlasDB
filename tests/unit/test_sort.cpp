#include "atlasdb/execution/operators/sort.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace atlasdb {
namespace {

class VectorSource final : public ExecutionOperator {
public:
    VectorSource(
        Schema schema,
        std::vector<std::vector<Value>> rows
    )
        : schema_(std::move(schema)),
          rows_(std::move(rows)) {}

    [[nodiscard]] auto schema() const noexcept -> const Schema& override {
        return schema_;
    }

    auto next(std::vector<Value>& output_row) -> bool override {
        if (index_ >= rows_.size()) {
            output_row.clear();
            return false;
        }

        output_row = rows_[index_++];
        return true;
    }

private:
    Schema schema_;
    std::vector<std::vector<Value>> rows_;
    std::size_t index_{0};
};

auto make_source(
    std::vector<Column> columns,
    std::vector<std::vector<Value>> rows
) -> std::unique_ptr<ExecutionOperator> {
    return std::make_unique<VectorSource>(
        Schema(std::move(columns)),
        std::move(rows)
    );
}

TEST(SortTest, SortsInt64Ascending) {
    auto input = make_source(
        {{"n", DataType::Int64}},
        {{Value(3)}, {Value(1)}, {Value(2)}}
    );

    Sort sort(std::move(input), {{0}});
    std::vector<Value> row;

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 2);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 3);

    EXPECT_FALSE(sort.next(row));
}

TEST(SortTest, SortsInt64Descending) {
    auto input = make_source(
        {{"n", DataType::Int64}},
        {{Value(1)}, {Value(3)}, {Value(2)}}
    );

    Sort sort(
        std::move(input),
        {{0, SortDirection::Descending}}
    );

    std::vector<Value> row;
    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 3);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 2);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);
}

TEST(SortTest, SortsStringsLexicographically) {
    auto input = make_source(
        {{"word", DataType::String}},
        {{Value("pear")}, {Value("apple")}, {Value("banana")}}
    );

    Sort sort(std::move(input), {{0}});
    std::vector<Value> row;

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::string>(), "apple");

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::string>(), "banana");

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::string>(), "pear");
}

TEST(SortTest, SortsByMultipleColumns) {
    auto input = make_source(
        {
            {"group", DataType::Int64},
            {"score", DataType::Int64}
        },
        {
            {Value(1), Value(10)},
            {Value(2), Value(5)},
            {Value(1), Value(20)},
            {Value(1), Value(15)}
        }
    );

    Sort sort(
        std::move(input),
        {
            {0, SortDirection::Ascending},
            {1, SortDirection::Descending}
        }
    );

    std::vector<Value> row;

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);
    EXPECT_EQ(row[1].as<std::int64_t>(), 20);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);
    EXPECT_EQ(row[1].as<std::int64_t>(), 15);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);
    EXPECT_EQ(row[1].as<std::int64_t>(), 10);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 2);
}

TEST(SortTest, PlacesNullsLastInAscendingOrder) {
    auto input = make_source(
        {{"n", DataType::Int64}},
        {
            {Value::null()},
            {Value(2)},
            {Value::null()},
            {Value(1)}
        }
    );

    Sort sort(
        std::move(input),
        {{0, SortDirection::Ascending, NullPlacement::Last}}
    );

    std::vector<Value> row;
    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 2);

    ASSERT_TRUE(sort.next(row));
    EXPECT_TRUE(row[0].is_null());

    ASSERT_TRUE(sort.next(row));
    EXPECT_TRUE(row[0].is_null());
}

TEST(SortTest, PlacesNullsFirstEvenInDescendingOrder) {
    auto input = make_source(
        {{"n", DataType::Int64}},
        {
            {Value(1)},
            {Value::null()},
            {Value(3)},
            {Value::null()}
        }
    );

    Sort sort(
        std::move(input),
        {{0, SortDirection::Descending, NullPlacement::First}}
    );

    std::vector<Value> row;
    ASSERT_TRUE(sort.next(row));
    EXPECT_TRUE(row[0].is_null());

    ASSERT_TRUE(sort.next(row));
    EXPECT_TRUE(row[0].is_null());

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 3);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), 1);
}

TEST(SortTest, PreservesInputOrderForEqualKeys) {
    auto input = make_source(
        {
            {"key", DataType::Int64},
            {"tag", DataType::String}
        },
        {
            {Value(1), Value("first")},
            {Value(1), Value("second")},
            {Value(1), Value("third")}
        }
    );

    Sort sort(std::move(input), {{0}});
    std::vector<Value> row;

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[1].as<std::string>(), "first");

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[1].as<std::string>(), "second");

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[1].as<std::string>(), "third");
}

TEST(SortTest, SortsBooleanValues) {
    auto input = make_source(
        {{"flag", DataType::Boolean}},
        {{Value(true)}, {Value(false)}}
    );

    Sort sort(std::move(input), {{0}});
    std::vector<Value> row;

    ASSERT_TRUE(sort.next(row));
    EXPECT_FALSE(row[0].as<bool>());

    ASSERT_TRUE(sort.next(row));
    EXPECT_TRUE(row[0].as<bool>());
}

TEST(SortTest, PreservesExactInt64Ordering) {
    const auto largest = std::numeric_limits<std::int64_t>::max();
    const auto smaller = largest - 1;

    auto input = make_source(
        {{"n", DataType::Int64}},
        {{Value(largest)}, {Value(smaller)}}
    );

    Sort sort(std::move(input), {{0}});
    std::vector<Value> row;

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), smaller);

    ASSERT_TRUE(sort.next(row));
    EXPECT_EQ(row[0].as<std::int64_t>(), largest);
}

TEST(SortTest, HandlesEmptyInput) {
    auto input = make_source(
        {{"n", DataType::Int64}},
        {}
    );

    Sort sort(std::move(input), {{0}});
    std::vector<Value> row{Value(99)};

    EXPECT_FALSE(sort.next(row));
    EXPECT_TRUE(row.empty());
}

TEST(SortTest, RejectsNullInput) {
    EXPECT_THROW(
        Sort(std::unique_ptr<ExecutionOperator>{}, {{0}}),
        std::invalid_argument
    );
}

TEST(SortTest, RejectsEmptySortKeys) {
    auto input = make_source(
        {{"n", DataType::Int64}},
        {}
    );

    EXPECT_THROW(
        Sort(std::move(input), {}),
        std::invalid_argument
    );
}

TEST(SortTest, RejectsOutOfRangeColumn) {
    auto input = make_source(
        {{"n", DataType::Int64}},
        {}
    );

    EXPECT_THROW(
        Sort(std::move(input), {{1}}),
        std::invalid_argument
    );
}

TEST(SortTest, PreservesOutputSchema) {
    auto input = make_source(
        {
            {"id", DataType::Int64},
            {"name", DataType::String}
        },
        {}
    );

    Sort sort(std::move(input), {{0}});

    EXPECT_EQ(sort.schema().column_count(), 2U);
    EXPECT_EQ(sort.schema().column_at(0).name, "id");
    EXPECT_EQ(sort.schema().column_at(1).name, "name");
}

} // namespace
} // namespace atlasdb
