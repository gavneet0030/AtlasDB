#pragma once

#include "atlasdb/parser/lexer.hpp"
#include "atlasdb/catalog/schema.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace atlasdb {

enum class ExpressionKind {
    Literal,
    Column,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    And,
    Or,
    Not
};

struct ParsedExpression {
    ExpressionKind kind;
    TokenKind literal_kind{TokenKind::End};
    std::string value;
    std::unique_ptr<ParsedExpression> left;
    std::unique_ptr<ParsedExpression> right;
};

struct OrderByItem {
    std::string column;
    bool ascending{true};
    bool is_aggregate{false};
    std::string aggregate_function;
    std::string aggregate_column;
    bool aggregate_star_argument{false};
};

struct AggregateSelection {
    std::string function;
    std::string column;
    bool star_argument{false};
};

struct SelectQuery {
    bool select_all{false};
    std::vector<std::string> columns;
    std::vector<AggregateSelection> aggregates;
    std::string table;
    std::string join_table;
    std::unique_ptr<ParsedExpression> join_condition;
    std::unique_ptr<ParsedExpression> where;
    std::vector<std::string> group_by;
    std::vector<OrderByItem> order_by;
};

enum class StatementKind {
    Select,
    CreateTable,
    Insert
};

struct ParsedLiteral {
    TokenKind kind{TokenKind::End};
    std::string value;
};

struct CreateTableStatement {
    std::string table_name;
    std::vector<Column> columns;
};

struct InsertStatement {
    std::string table_name;
    std::vector<std::string> columns;
    std::vector<std::vector<ParsedLiteral>> rows;
};

struct SqlStatement {
    StatementKind kind{StatementKind::Select};
    SelectQuery select;
    CreateTableStatement create_table;
    InsertStatement insert;
};

class Parser {
public:
    explicit Parser(std::string_view sql);

    [[nodiscard]] auto parse() -> SelectQuery;
    [[nodiscard]] auto parse_statement() -> SqlStatement;


private:
    [[nodiscard]] auto parse_create_table() -> SqlStatement;
    [[nodiscard]] auto parse_insert() -> SqlStatement;
    [[nodiscard]] auto parse_literal() -> ParsedLiteral;

    [[nodiscard]] auto peek() const -> const Token&;
    [[nodiscard]] auto check(TokenKind kind) const -> bool;
    [[nodiscard]] auto match(TokenKind kind) -> bool;
    [[nodiscard]] auto consume(TokenKind kind,
                               std::string_view message) -> Token;

    [[nodiscard]] auto parse_or() -> std::unique_ptr<ParsedExpression>;
    [[nodiscard]] auto parse_and() -> std::unique_ptr<ParsedExpression>;
    [[nodiscard]] auto parse_not() -> std::unique_ptr<ParsedExpression>;
    [[nodiscard]] auto parse_comparison()
        -> std::unique_ptr<ParsedExpression>;
    [[nodiscard]] auto parse_operand()
        -> std::unique_ptr<ParsedExpression>;

    [[nodiscard]] auto make_node(
        ExpressionKind kind,
        std::unique_ptr<ParsedExpression> left,
        std::unique_ptr<ParsedExpression> right = nullptr)
        -> std::unique_ptr<ParsedExpression>;

    [[noreturn]] void error(std::string_view message) const;

    std::vector<Token> tokens_;
    std::size_t current_{0};
};

}  // namespace atlasdb
