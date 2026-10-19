#include "atlasdb/execution/expressions/column_reference_expression.hpp"
#include "atlasdb/execution/expressions/comparison_expression.hpp"
#include "atlasdb/execution/expressions/literal_expression.hpp"
#include "atlasdb/execution/expressions/logical_expression.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace atlasdb {
namespace {

TEST(LiteralExpressionTest, ReturnsIntegerLiteral)
{
    LiteralExpression expression(Value(std::int64_t{42}));
    const std::vector<Value> row;

    const Value result = expression.evaluate(row);

    ASSERT_FALSE(result.is_null());
    EXPECT_EQ(result.as<std::int64_t>(), 42);
}

TEST(LiteralExpressionTest, ReturnsStringLiteral)
{
    LiteralExpression expression(Value("AtlasDB"));
    const std::vector<Value> row;

    const Value result = expression.evaluate(row);

    EXPECT_EQ(result.as<std::string>(), "AtlasDB");
}

TEST(LiteralExpressionTest, ReturnsNullLiteral)
{
    LiteralExpression expression(Value::null());
    const std::vector<Value> row;

    EXPECT_TRUE(expression.evaluate(row).is_null());
}

TEST(ColumnReferenceExpressionTest, ReturnsValueAtColumnIndex)
{
    const std::vector<Value> row{
        Value(std::int64_t{7}),
        Value("AtlasDB"),
        Value(true)
    };

    ColumnReferenceExpression expression(1);

    EXPECT_EQ(expression.evaluate(row).as<std::string>(), "AtlasDB");
}

TEST(ColumnReferenceExpressionTest, ReturnsIntegerFromFirstColumn)
{
    const std::vector<Value> row{
        Value(std::int64_t{99}),
        Value("example")
    };

    ColumnReferenceExpression expression(0);

    EXPECT_EQ(expression.evaluate(row).as<std::int64_t>(), 99);
}

TEST(ColumnReferenceExpressionTest, PreservesNullValue)
{
    const std::vector<Value> row{
        Value(std::int64_t{1}),
        Value::null()
    };

    ColumnReferenceExpression expression(1);

    EXPECT_TRUE(expression.evaluate(row).is_null());
}

TEST(ColumnReferenceExpressionTest, ThrowsForOutOfRangeIndex)
{
    const std::vector<Value> row{Value(std::int64_t{1})};
    ColumnReferenceExpression expression(5);

    EXPECT_THROW(expression.evaluate(row), std::out_of_range);
}

TEST(ComparisonExpressionTest, ComparesEqualIntegers)
{
    ComparisonExpression expression(
        ComparisonOperator::Equal,
        std::make_unique<LiteralExpression>(Value(10)),
        std::make_unique<LiteralExpression>(Value(10))
    );

    EXPECT_TRUE(expression.evaluate({}).as<bool>());
}

TEST(ComparisonExpressionTest, ComparesGreaterIntegers)
{
    ComparisonExpression expression(
        ComparisonOperator::GreaterThan,
        std::make_unique<LiteralExpression>(Value(20)),
        std::make_unique<LiteralExpression>(Value(10))
    );

    EXPECT_TRUE(expression.evaluate({}).as<bool>());
}

TEST(ComparisonExpressionTest, ComparesIntegerAndDouble)
{
    ComparisonExpression expression(
        ComparisonOperator::LessThan,
        std::make_unique<LiteralExpression>(Value(5)),
        std::make_unique<LiteralExpression>(Value(5.5))
    );

    EXPECT_TRUE(expression.evaluate({}).as<bool>());
}

TEST(ComparisonExpressionTest, ComparesStringsLexicographically)
{
    ComparisonExpression expression(
        ComparisonOperator::LessThan,
        std::make_unique<LiteralExpression>(Value("Atlas")),
        std::make_unique<LiteralExpression>(Value("Database"))
    );

    EXPECT_TRUE(expression.evaluate({}).as<bool>());
}

TEST(ComparisonExpressionTest, ComparesBooleanEquality)
{
    ComparisonExpression expression(
        ComparisonOperator::Equal,
        std::make_unique<LiteralExpression>(Value(true)),
        std::make_unique<LiteralExpression>(Value(true))
    );

    EXPECT_TRUE(expression.evaluate({}).as<bool>());
}

TEST(ComparisonExpressionTest, ReturnsNullWhenOperandIsNull)
{
    ComparisonExpression expression(
        ComparisonOperator::Equal,
        std::make_unique<LiteralExpression>(Value::null()),
        std::make_unique<LiteralExpression>(Value(10))
    );

    EXPECT_TRUE(expression.evaluate({}).is_null());
}

TEST(ComparisonExpressionTest, RejectsIncompatibleTypes)
{
    ComparisonExpression expression(
        ComparisonOperator::Equal,
        std::make_unique<LiteralExpression>(Value(10)),
        std::make_unique<LiteralExpression>(Value("10"))
    );

    EXPECT_THROW(expression.evaluate({}), std::invalid_argument);
}

TEST(ComparisonExpressionTest, RejectsBooleanOrdering)
{
    ComparisonExpression expression(
        ComparisonOperator::LessThan,
        std::make_unique<LiteralExpression>(Value(false)),
        std::make_unique<LiteralExpression>(Value(true))
    );

    EXPECT_THROW(expression.evaluate({}), std::invalid_argument);
}


// Logical expression tests

TEST(LogicalExpressionTest, AndReturnsTrueWhenBothOperandsAreTrue)
{
    LogicalExpression expression(
        LogicalOperator::And,
        std::make_unique<LiteralExpression>(Value(true)),
        std::make_unique<LiteralExpression>(Value(true)));

    EXPECT_TRUE(expression.evaluate({}).as<bool>());
}

TEST(LogicalExpressionTest, AndReturnsFalseWhenEitherOperandIsFalse)
{
    LogicalExpression expression(
        LogicalOperator::And,
        std::make_unique<LiteralExpression>(Value(true)),
        std::make_unique<LiteralExpression>(Value(false)));

    EXPECT_FALSE(expression.evaluate({}).as<bool>());
}

TEST(LogicalExpressionTest, OrReturnsTrueWhenEitherOperandIsTrue)
{
    LogicalExpression expression(
        LogicalOperator::Or,
        std::make_unique<LiteralExpression>(Value(false)),
        std::make_unique<LiteralExpression>(Value(true)));

    EXPECT_TRUE(expression.evaluate({}).as<bool>());
}

TEST(LogicalExpressionTest, OrReturnsFalseWhenBothOperandsAreFalse)
{
    LogicalExpression expression(
        LogicalOperator::Or,
        std::make_unique<LiteralExpression>(Value(false)),
        std::make_unique<LiteralExpression>(Value(false)));

    EXPECT_FALSE(expression.evaluate({}).as<bool>());
}

TEST(LogicalExpressionTest, NotInvertsBoolean)
{
    LogicalExpression expression(
        LogicalOperator::Not,
        std::make_unique<LiteralExpression>(Value(true)));

    EXPECT_FALSE(expression.evaluate({}).as<bool>());
}

TEST(LogicalExpressionTest, AndReturnsFalseForFalseAndNull)
{
    LogicalExpression expression(
        LogicalOperator::And,
        std::make_unique<LiteralExpression>(Value(false)),
        std::make_unique<LiteralExpression>(Value::null()));

    const Value result = expression.evaluate({});
    ASSERT_FALSE(result.is_null());
    EXPECT_FALSE(result.as<bool>());
}

TEST(LogicalExpressionTest, AndReturnsNullForTrueAndNull)
{
    LogicalExpression expression(
        LogicalOperator::And,
        std::make_unique<LiteralExpression>(Value(true)),
        std::make_unique<LiteralExpression>(Value::null()));

    EXPECT_TRUE(expression.evaluate({}).is_null());
}

TEST(LogicalExpressionTest, OrReturnsTrueForTrueAndNull)
{
    LogicalExpression expression(
        LogicalOperator::Or,
        std::make_unique<LiteralExpression>(Value(true)),
        std::make_unique<LiteralExpression>(Value::null()));

    const Value result = expression.evaluate({});
    ASSERT_FALSE(result.is_null());
    EXPECT_TRUE(result.as<bool>());
}

TEST(LogicalExpressionTest, OrReturnsNullForFalseOrNull)
{
    LogicalExpression expression(
        LogicalOperator::Or,
        std::make_unique<LiteralExpression>(Value(false)),
        std::make_unique<LiteralExpression>(Value::null()));

    EXPECT_TRUE(expression.evaluate({}).is_null());
}

TEST(LogicalExpressionTest, NotReturnsNullForNullOperand)
{
    LogicalExpression expression(
        LogicalOperator::Not,
        std::make_unique<LiteralExpression>(Value::null()));

    EXPECT_TRUE(expression.evaluate({}).is_null());
}

TEST(LogicalExpressionTest, RejectsNonBooleanOperand)
{
    LogicalExpression expression(
        LogicalOperator::Not,
        std::make_unique<LiteralExpression>(Value(42)));

    EXPECT_THROW(expression.evaluate({}), std::invalid_argument);
}

TEST(LogicalExpressionTest, RejectsNullaryAnd)
{
    EXPECT_THROW(
        LogicalExpression(
            LogicalOperator::And,
            std::unique_ptr<Expression>{},
            std::make_unique<LiteralExpression>(Value(true))),
        std::invalid_argument);
}

TEST(LogicalExpressionTest, AndShortCircuitsWhenLeftIsFalse)
{
    LogicalExpression expression(
        LogicalOperator::And,
        std::make_unique<LiteralExpression>(Value(false)),
        std::make_unique<ColumnReferenceExpression>(99));

    const Value result = expression.evaluate({});

    ASSERT_FALSE(result.is_null());
    EXPECT_FALSE(result.as<bool>());
}

TEST(LogicalExpressionTest, OrShortCircuitsWhenLeftIsTrue)
{
    LogicalExpression expression(
        LogicalOperator::Or,
        std::make_unique<LiteralExpression>(Value(true)),
        std::make_unique<ColumnReferenceExpression>(99));

    const Value result = expression.evaluate({});

    ASSERT_FALSE(result.is_null());
    EXPECT_TRUE(result.as<bool>());
}

TEST(ComparisonExpressionTest, RejectsNullOperand)
{
    EXPECT_THROW(
        ComparisonExpression(
            ComparisonOperator::Equal,
            std::unique_ptr<Expression>{},
            std::make_unique<LiteralExpression>(Value(1))),
        std::invalid_argument);
}

}  // namespace
}  // namespace atlasdb
