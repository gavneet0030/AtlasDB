#include "atlasdb/parser/lexer.hpp"

#include <cctype>
#include <stdexcept>
#include <string>

namespace atlasdb {
namespace {

auto upper_ascii(std::string text) -> std::string {
    for (char& ch : text) {
        ch = static_cast<char>(
            std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}

auto keyword_kind(const std::string& text) -> TokenKind {
    const auto word = upper_ascii(text);

    if (word == "CREATE") return TokenKind::Create;
    if (word == "TABLE") return TokenKind::Table;
    if (word == "INSERT") return TokenKind::Insert;
    if (word == "INTO") return TokenKind::Into;
    if (word == "VALUES") return TokenKind::Values;

    if (word == "INT64") return TokenKind::Int64Type;
    if (word == "DOUBLE") return TokenKind::DoubleType;
    if (word == "BOOLEAN") return TokenKind::BooleanType;
    if (word == "STRING") return TokenKind::StringType;

    if (word == "SELECT") return TokenKind::Select;
    if (word == "FROM") return TokenKind::From;
    if (word == "INNER") return TokenKind::Inner;
    if (word == "JOIN") return TokenKind::Join;
    if (word == "ON") return TokenKind::On;
    if (word == "WHERE") return TokenKind::Where;
    if (word == "GROUP") return TokenKind::Group;
    if (word == "ORDER") return TokenKind::Order;
    if (word == "BY") return TokenKind::By;
    if (word == "ASC") return TokenKind::Asc;
    if (word == "DESC") return TokenKind::Desc;
    if (word == "AND") return TokenKind::And;
    if (word == "OR") return TokenKind::Or;
    if (word == "NOT") return TokenKind::Not;
    if (word == "TRUE") return TokenKind::True;
    if (word == "FALSE") return TokenKind::False;
    if (word == "NULL") return TokenKind::Null;

    return TokenKind::Identifier;
}

}  // namespace

Lexer::Lexer(std::string_view input) : input_(input) {}

auto Lexer::peek() const -> char {
    return position_ < input_.size() ? input_[position_] : '\0';
}

auto Lexer::advance() -> char {
    return position_ < input_.size() ? input_[position_++] : '\0';
}

auto Lexer::match(char expected) -> bool {
    if (peek() != expected) return false;
    ++position_;
    return true;
}

void Lexer::skip_whitespace() {
    while (position_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(peek()))) {
        ++position_;
    }
}

auto Lexer::scan_token() -> Token {
    skip_whitespace();
    const auto start = position_;

    if (position_ >= input_.size()) {
        return {TokenKind::End, "", position_};
    }

    const char ch = advance();

    if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
        while (std::isalnum(static_cast<unsigned char>(peek())) ||
               peek() == '_' ||
               (peek() == '.' &&
                position_ + 1 < input_.size() &&
                (std::isalpha(static_cast<unsigned char>(input_[position_ + 1])) ||
                 input_[position_ + 1] == '_'))) {
            static_cast<void>(advance());
        }

        std::string text(input_.substr(start, position_ - start));
        return {keyword_kind(text), text, start};
    }

    if (std::isdigit(static_cast<unsigned char>(ch))) {
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            static_cast<void>(advance());
        }

        bool floating = false;
        if (peek() == '.' &&
            position_ + 1 < input_.size() &&
            std::isdigit(static_cast<unsigned char>(input_[position_ + 1]))) {
            floating = true;
            static_cast<void>(advance());
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                static_cast<void>(advance());
            }
        }

        return {
            floating ? TokenKind::FloatingLiteral : TokenKind::IntegerLiteral,
            std::string(input_.substr(start, position_ - start)),
            start
        };
    }

    if (ch == '\'') {
        std::string value;

        while (position_ < input_.size()) {
            const char current = advance();

            if (current == '\'') {
                if (peek() == '\'') {
                    static_cast<void>(advance());
                    value.push_back('\'');
                    continue;
                }

                return {TokenKind::StringLiteral, value, start};
            }

            value.push_back(current);
        }

        throw std::runtime_error(
            "Unterminated string literal at position " +
            std::to_string(start));
    }

    switch (ch) {
    case '=': return {TokenKind::Equal, "=", start};
    case '!':
        if (match('=')) return {TokenKind::NotEqual, "!=", start};
        break;
    case '<':
        if (match('>')) return {TokenKind::NotEqual, "<>", start};
        if (match('=')) return {TokenKind::LessEqual, "<=", start};
        return {TokenKind::Less, "<", start};
    case '>':
        if (match('=')) return {TokenKind::GreaterEqual, ">=", start};
        return {TokenKind::Greater, ">", start};
    case ',': return {TokenKind::Comma, ",", start};
    case '*': return {TokenKind::Star, "*", start};
    case ';': return {TokenKind::Semicolon, ";", start};
    case '(': return {TokenKind::LeftParen, "(", start};
    case ')': return {TokenKind::RightParen, ")", start};
    default: break;
    }

    throw std::runtime_error(
        "Unexpected character at position " + std::to_string(start));
}

auto Lexer::tokenize() -> std::vector<Token> {
    std::vector<Token> tokens;

    while (true) {
        auto token = scan_token();
        tokens.push_back(token);

        if (token.kind == TokenKind::End) {
            break;
        }
    }

    return tokens;
}

}  // namespace atlasdb
