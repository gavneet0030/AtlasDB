#include "atlasdb/parser/lexer.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace atlasdb {
namespace {

TEST(LexerTest, TokenizesBasicSelectStatement) {
    Lexer lexer("SELECT name FROM employees;");
    const auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 6U);
    EXPECT_EQ(tokens[0].kind, TokenKind::Select);
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "name");
    EXPECT_EQ(tokens[2].kind, TokenKind::From);
    EXPECT_EQ(tokens[3].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[3].lexeme, "employees");
    EXPECT_EQ(tokens[4].kind, TokenKind::Semicolon);
    EXPECT_EQ(tokens[5].kind, TokenKind::End);
}

TEST(LexerTest, RecognizesKeywordsCaseInsensitively) {
    Lexer lexer("select FROM WhErE GROUP order BY ASC desc");
    const auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 9U);
    EXPECT_EQ(tokens[0].kind, TokenKind::Select);
    EXPECT_EQ(tokens[1].kind, TokenKind::From);
    EXPECT_EQ(tokens[2].kind, TokenKind::Where);
    EXPECT_EQ(tokens[3].kind, TokenKind::Group);
    EXPECT_EQ(tokens[4].kind, TokenKind::Order);
    EXPECT_EQ(tokens[5].kind, TokenKind::By);
    EXPECT_EQ(tokens[6].kind, TokenKind::Asc);
    EXPECT_EQ(tokens[7].kind, TokenKind::Desc);
    EXPECT_EQ(tokens[8].kind, TokenKind::End);
}

TEST(LexerTest, TokenizesIntegerAndFloatingLiterals) {
    Lexer lexer("42 3.14 0");
    const auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[0].kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(tokens[0].lexeme, "42");
    EXPECT_EQ(tokens[1].kind, TokenKind::FloatingLiteral);
    EXPECT_EQ(tokens[1].lexeme, "3.14");
    EXPECT_EQ(tokens[2].kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(tokens[2].lexeme, "0");
    EXPECT_EQ(tokens[3].kind, TokenKind::End);
}

TEST(LexerTest, HandlesEscapedSingleQuotesInStrings) {
    Lexer lexer("'O''Brien'");
    const auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0].kind, TokenKind::StringLiteral);
    EXPECT_EQ(tokens[0].lexeme, "O'Brien");
}

TEST(LexerTest, TokenizesComparisonOperatorsAndPunctuation) {
    Lexer lexer("= != <> < > , * ( ) ;");
    const auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 11U);
    EXPECT_EQ(tokens[0].kind, TokenKind::Equal);
    EXPECT_EQ(tokens[1].kind, TokenKind::NotEqual);
    EXPECT_EQ(tokens[2].kind, TokenKind::NotEqual);
    EXPECT_EQ(tokens[3].kind, TokenKind::Less);
    EXPECT_EQ(tokens[4].kind, TokenKind::Greater);
    EXPECT_EQ(tokens[5].kind, TokenKind::Comma);
    EXPECT_EQ(tokens[6].kind, TokenKind::Star);
    EXPECT_EQ(tokens[7].kind, TokenKind::LeftParen);
    EXPECT_EQ(tokens[8].kind, TokenKind::RightParen);
    EXPECT_EQ(tokens[9].kind, TokenKind::Semicolon);
    EXPECT_EQ(tokens[10].kind, TokenKind::End);
}

TEST(LexerTest, RecognizesLogicalAndLiteralKeywords) {
    Lexer lexer("AND OR NOT TRUE FALSE NULL");
    const auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 7U);
    EXPECT_EQ(tokens[0].kind, TokenKind::And);
    EXPECT_EQ(tokens[1].kind, TokenKind::Or);
    EXPECT_EQ(tokens[2].kind, TokenKind::Not);
    EXPECT_EQ(tokens[3].kind, TokenKind::True);
    EXPECT_EQ(tokens[4].kind, TokenKind::False);
    EXPECT_EQ(tokens[5].kind, TokenKind::Null);
    EXPECT_EQ(tokens[6].kind, TokenKind::End);
}

TEST(LexerTest, RejectsUnterminatedString) {
    Lexer lexer("'unfinished");

    EXPECT_THROW(static_cast<void>(lexer.tokenize()), std::runtime_error);
}

TEST(LexerTest, TokenizesLessThanOrEqualOperator) {
    Lexer lexer("salary <= 50000");
    const auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[1].kind, TokenKind::LessEqual);
    EXPECT_EQ(tokens[1].lexeme, "<=");
}

TEST(LexerTest, TokenizesGreaterThanOrEqualOperator) {
    Lexer lexer("salary >= 50000");
    const auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[1].kind, TokenKind::GreaterEqual);
    EXPECT_EQ(tokens[1].lexeme, ">=");
}

TEST(LexerTest, RejectsUnexpectedCharacters) {
    Lexer lexer("SELECT @ FROM employees");

    EXPECT_THROW(static_cast<void>(lexer.tokenize()), std::runtime_error);
}

TEST(LexerTest, EmitsEndTokenForEmptyInput) {
    Lexer lexer("");
    const auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].kind, TokenKind::End);
}

}  // namespace
}  // namespace atlasdb
