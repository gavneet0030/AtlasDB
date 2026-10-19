#pragma once

#include <cstddef>
#include <string>

namespace atlasdb {

enum class TokenKind {
    Create, Table, Insert, Into, Values,
    Int64Type, DoubleType, BooleanType, StringType,
    Select, From, Inner, Join, On, Where,
    Group, Order, By, Asc, Desc,
    And, Or, Not, True, False, Null,
    Identifier, IntegerLiteral, FloatingLiteral, StringLiteral,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    Comma, Star, Semicolon, LeftParen, RightParen,
    End
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    std::size_t position;
};

}  // namespace atlasdb
