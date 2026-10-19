#include "atlasdb/parser/parser.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace atlasdb {

Parser::Parser(std::string_view sql) {
    Lexer lexer(sql);
    tokens_ = lexer.tokenize();
}

auto Parser::peek() const -> const Token& {
    return tokens_[current_];
}

auto Parser::check(TokenKind kind) const -> bool {
    return peek().kind == kind;
}

auto Parser::match(TokenKind kind) -> bool {
    if (!check(kind)) {
        return false;
    }
    ++current_;
    return true;
}

auto Parser::consume(TokenKind kind, std::string_view message) -> Token {
    if (check(kind)) {
        return tokens_[current_++];
    }
    error(message);
}

[[noreturn]] void Parser::error(std::string_view message) const {
    throw std::runtime_error(
        "SQL parse error at position " + std::to_string(peek().position) +
        ": " + std::string(message));
}

auto Parser::make_node(
    ExpressionKind kind,
    std::unique_ptr<ParsedExpression> left,
    std::unique_ptr<ParsedExpression> right)
    -> std::unique_ptr<ParsedExpression> {
    auto node = std::make_unique<ParsedExpression>();
    node->kind = kind;
    node->left = std::move(left);
    node->right = std::move(right);
    return node;
}

auto Parser::parse() -> SelectQuery {
    SelectQuery query;

    static_cast<void>(consume(TokenKind::Select, "expected SELECT"));

    if (match(TokenKind::Star)) {
        query.select_all = true;
    } else {
        do {
            const bool looks_like_aggregate =
                check(TokenKind::Identifier) &&
                current_ + 1 < tokens_.size() &&
                tokens_[current_ + 1].kind == TokenKind::LeftParen;

            if (looks_like_aggregate) {
                AggregateSelection aggregate;
                aggregate.function = tokens_[current_++].lexeme;

                static_cast<void>(consume(
                    TokenKind::LeftParen,
                    "expected '(' after aggregate function"));

                if (match(TokenKind::Star)) {
                    aggregate.star_argument = true;
                } else {
                    aggregate.column = consume(
                        TokenKind::Identifier,
                        "expected a column name or * inside aggregate")
                        .lexeme;
                }

                static_cast<void>(consume(
                    TokenKind::RightParen,
                    "expected ')' after aggregate argument"));

                query.aggregates.push_back(std::move(aggregate));
            } else {
                query.columns.push_back(consume(
                    TokenKind::Identifier,
                    "expected a column name, aggregate, or * after SELECT")
                    .lexeme);
            }
        } while (match(TokenKind::Comma));

    }

    static_cast<void>(
        consume(TokenKind::From, "expected FROM after selected columns"));
    query.table =
        consume(TokenKind::Identifier, "expected a table name after FROM")
            .lexeme;

    if (match(TokenKind::Inner)) {
        static_cast<void>(consume(TokenKind::Join, "expected JOIN after INNER"));
        query.join_table =
            consume(TokenKind::Identifier, "expected table name after JOIN")
                .lexeme;
        static_cast<void>(consume(TokenKind::On, "expected ON after JOIN table"));
        query.join_condition = parse_or();
        if (!query.join_condition) {
            error("expected a JOIN condition after ON");
        }
    } else if (check(TokenKind::Join) || check(TokenKind::On)) {
        error("expected INNER JOIN ... ON ...");
    }

    if (match(TokenKind::Where)) {
        query.where = parse_or();
    }

    if (match(TokenKind::Group)) {
        static_cast<void>(consume(TokenKind::By, "expected BY after GROUP"));

        do {
            query.group_by.push_back(
                consume(TokenKind::Identifier,
                        "expected a column name after GROUP BY").lexeme);
        } while (match(TokenKind::Comma));
    }

    if (!query.columns.empty() && !query.aggregates.empty() &&
        query.group_by.empty()) {
        error("ordinary columns selected with aggregates require GROUP BY");
    }


    if (match(TokenKind::Order)) {
        static_cast<void>(consume(TokenKind::By, "expected BY after ORDER"));

        do {
            OrderByItem item;

            const bool looks_like_aggregate =
                check(TokenKind::Identifier) &&
                current_ + 1 < tokens_.size() &&
                tokens_[current_ + 1].kind == TokenKind::LeftParen;

            if (looks_like_aggregate) {
                item.is_aggregate = true;
                item.aggregate_function =
                    consume(TokenKind::Identifier,
                            "expected aggregate function after ORDER BY").lexeme;

                static_cast<void>(consume(
                    TokenKind::LeftParen,
                    "expected '(' after ORDER BY aggregate"));

                if (match(TokenKind::Star)) {
                    item.aggregate_star_argument = true;
                    item.column = "*";
                } else {
                    item.aggregate_column =
                        consume(TokenKind::Identifier,
                                "expected aggregate column or * after ORDER BY")
                            .lexeme;
                    item.column = item.aggregate_column;
                }

                static_cast<void>(consume(
                    TokenKind::RightParen,
                    "expected ')' after ORDER BY aggregate"));
            } else {
                item.column =
                    consume(TokenKind::Identifier,
                            "expected a column name or aggregate after ORDER BY")
                        .lexeme;
            }

            if (match(TokenKind::Asc)) {
                item.ascending = true;
            } else if (match(TokenKind::Desc)) {
                item.ascending = false;
            }

            query.order_by.push_back(std::move(item));
        } while (match(TokenKind::Comma));
    }

    static_cast<void>(match(TokenKind::Semicolon));
    static_cast<void>(
        consume(TokenKind::End, "unexpected token after the SQL query"));

    return query;
}

auto Parser::parse_statement() -> SqlStatement {
    if (check(TokenKind::Select)) {
        SqlStatement statement;
        statement.kind = StatementKind::Select;
        statement.select = parse();
        return statement;
    }

    if (check(TokenKind::Create)) {
        return parse_create_table();
    }

    if (check(TokenKind::Insert)) {
        return parse_insert();
    }

    error("expected SELECT, CREATE TABLE, or INSERT INTO");
}

auto Parser::parse_create_table() -> SqlStatement {
    SqlStatement statement;
    statement.kind = StatementKind::CreateTable;

    static_cast<void>(consume(TokenKind::Create, "expected CREATE"));
    static_cast<void>(consume(TokenKind::Table, "expected TABLE after CREATE"));

    auto& create = statement.create_table;
    create.table_name =
        consume(TokenKind::Identifier, "expected table name").lexeme;

    static_cast<void>(consume(TokenKind::LeftParen,
                              "expected '(' after table name"));

    do {
        Column column;
        column.name =
            consume(TokenKind::Identifier, "expected column name").lexeme;

        if (match(TokenKind::Int64Type)) {
            column.type = DataType::Int64;
        } else if (match(TokenKind::DoubleType)) {
            column.type = DataType::Double;
        } else if (match(TokenKind::BooleanType)) {
            column.type = DataType::Boolean;
        } else if (match(TokenKind::StringType)) {
            column.type = DataType::String;
        } else {
            error("expected column type INT64, DOUBLE, BOOLEAN, or STRING");
        }

        create.columns.push_back(std::move(column));
    } while (match(TokenKind::Comma));

    static_cast<void>(consume(TokenKind::RightParen,
                              "expected ')' after column definitions"));
    static_cast<void>(match(TokenKind::Semicolon));
    static_cast<void>(consume(TokenKind::End,
                              "unexpected token after CREATE TABLE"));

    return statement;
}

auto Parser::parse_literal() -> ParsedLiteral {
    if (check(TokenKind::IntegerLiteral) ||
        check(TokenKind::FloatingLiteral) ||
        check(TokenKind::StringLiteral) ||
        check(TokenKind::True) ||
        check(TokenKind::False) ||
        check(TokenKind::Null)) {
        const auto token = tokens_[current_++];
        return ParsedLiteral{token.kind, token.lexeme};
    }

    error("expected a literal value in INSERT");
}

auto Parser::parse_insert() -> SqlStatement {
    SqlStatement statement;
    statement.kind = StatementKind::Insert;

    static_cast<void>(consume(TokenKind::Insert, "expected INSERT"));
    static_cast<void>(consume(TokenKind::Into, "expected INTO after INSERT"));

    auto& insert = statement.insert;
    insert.table_name =
        consume(TokenKind::Identifier, "expected table name after INTO").lexeme;

    if (match(TokenKind::LeftParen)) {
        do {
            insert.columns.push_back(
                consume(TokenKind::Identifier,
                        "expected column name in INSERT column list").lexeme);
        } while (match(TokenKind::Comma));

        static_cast<void>(consume(TokenKind::RightParen,
                                  "expected ')' after INSERT column list"));
    }

    static_cast<void>(consume(TokenKind::Values, "expected VALUES in INSERT"));

    do {
        std::vector<ParsedLiteral> row;

        static_cast<void>(consume(TokenKind::LeftParen,
                                  "expected '(' before INSERT values"));

        row.push_back(parse_literal());
        while (match(TokenKind::Comma)) {
            row.push_back(parse_literal());
        }

        static_cast<void>(consume(TokenKind::RightParen,
                                  "expected ')' after INSERT values"));

        insert.rows.push_back(std::move(row));
    } while (match(TokenKind::Comma));

    static_cast<void>(match(TokenKind::Semicolon));
    static_cast<void>(consume(TokenKind::End,
                              "unexpected token after INSERT"));

    return statement;
}
// OR has lower precedence than AND.
auto Parser::parse_or() -> std::unique_ptr<ParsedExpression> {
    auto expression = parse_and();

    while (match(TokenKind::Or)) {
        expression = make_node(
            ExpressionKind::Or, std::move(expression), parse_and());
    }

    return expression;
}

auto Parser::parse_and() -> std::unique_ptr<ParsedExpression> {
    auto expression = parse_not();

    while (match(TokenKind::And)) {
        expression = make_node(
            ExpressionKind::And, std::move(expression), parse_not());
    }

    return expression;
}

auto Parser::parse_not() -> std::unique_ptr<ParsedExpression> {
    if (match(TokenKind::Not)) {
        return make_node(ExpressionKind::Not, parse_not());
    }

    if (match(TokenKind::LeftParen)) {
        auto expression = parse_or();
        static_cast<void>(consume(TokenKind::RightParen, "expected ')' after condition"));
        return expression;
    }

    return parse_comparison();
}

auto Parser::parse_comparison()
    -> std::unique_ptr<ParsedExpression> {
    auto left = parse_operand();

    ExpressionKind kind;
    if (match(TokenKind::Equal)) {
        kind = ExpressionKind::Equal;
    } else if (match(TokenKind::NotEqual)) {
        kind = ExpressionKind::NotEqual;
    } else if (match(TokenKind::LessEqual)) {
        kind = ExpressionKind::LessEqual;
    } else if (match(TokenKind::Less)) {
        kind = ExpressionKind::Less;
    } else if (match(TokenKind::GreaterEqual)) {
        kind = ExpressionKind::GreaterEqual;
    } else if (match(TokenKind::Greater)) {
        kind = ExpressionKind::Greater;
    } else {
        return left;
    }

    auto right = parse_operand();
    return make_node(kind, std::move(left), std::move(right));
}

auto Parser::parse_operand() -> std::unique_ptr<ParsedExpression> {
    if (check(TokenKind::Identifier)) {
        auto token = tokens_[current_++];
        auto node = std::make_unique<ParsedExpression>();
        node->kind = ExpressionKind::Column;
        node->value = std::move(token.lexeme);
        return node;
    }

    if (check(TokenKind::IntegerLiteral) ||
        check(TokenKind::FloatingLiteral) ||
        check(TokenKind::StringLiteral) ||
        check(TokenKind::True) ||
        check(TokenKind::False) ||
        check(TokenKind::Null)) {
        auto token = tokens_[current_++];
        auto node = std::make_unique<ParsedExpression>();
        node->kind = ExpressionKind::Literal;
        node->literal_kind = token.kind;
        node->value = std::move(token.lexeme);
        return node;
    }

    error("expected a column name or literal");
}

}  // namespace atlasdb
