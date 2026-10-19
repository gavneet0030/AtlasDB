#include "atlasdb/execution/database_sql_executor.hpp"

#include "atlasdb/parser/token.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace atlasdb {
namespace {

auto literal_to_value(const ParsedLiteral& literal) -> Value {
    try {
        switch (literal.kind) {
            case TokenKind::IntegerLiteral: {
                std::size_t consumed = 0;
                const auto parsed = std::stoll(literal.value, &consumed);
                if (consumed != literal.value.size()) {
                    throw std::invalid_argument("invalid integer literal");
                }
                if (parsed < std::numeric_limits<std::int64_t>::min() ||
                    parsed > std::numeric_limits<std::int64_t>::max()) {
                    throw std::out_of_range("integer literal is out of range");
                }
                return Value(static_cast<std::int64_t>(parsed));
            }

            case TokenKind::FloatingLiteral: {
                std::size_t consumed = 0;
                const double parsed = std::stod(literal.value, &consumed);
                if (consumed != literal.value.size()) {
                    throw std::invalid_argument("invalid floating-point literal");
                }
                return Value(parsed);
            }

            case TokenKind::StringLiteral:
                return Value(literal.value);

            case TokenKind::True:
                return Value(true);

            case TokenKind::False:
                return Value(false);

            case TokenKind::Null:
                return Value::null();

            default:
                throw std::invalid_argument("unsupported INSERT literal");
        }
    } catch (const std::invalid_argument&) {
        throw;
    } catch (const std::out_of_range&) {
        throw std::invalid_argument(
            "numeric INSERT literal is out of range: " + literal.value);
    }
}

auto make_insert_row(
    const std::vector<ParsedLiteral>& literals,
    const std::vector<std::size_t>& target_columns,
    const Schema& schema) -> std::vector<Value> {
    if (literals.size() != target_columns.size()) {
        throw std::invalid_argument(
            "INSERT row value count does not match the column count");
    }

    std::vector<Value> row(schema.column_count(), Value::null());

    for (std::size_t i = 0; i < literals.size(); ++i) {
        const std::size_t target = target_columns[i];
        Value value = literal_to_value(literals[i]);

        const auto& column = schema.column_at(target);
        if (!value.is_compatible_with(column.type)) {
            throw std::invalid_argument(
                "INSERT type mismatch for column '" + column.name +
                "': expected " + std::string(to_string(column.type)));
        }

        row[target] = std::move(value);
    }

    return row;
}

}  // namespace

auto DatabaseSqlExecutor::execute(
    std::string_view sql,
    DatabaseCatalog& catalog) -> DatabaseExecutionResult {
    Parser parser(sql);
    const SqlStatement statement = parser.parse_statement();

    DatabaseExecutionResult result;
    result.kind = statement.kind;

    switch (statement.kind) {
        case StatementKind::CreateTable: {
            const auto& create = statement.create_table;
            catalog.create_table(create.table_name, Schema(create.columns));
            result.message = "Table created: " + create.table_name;
            return result;
        }

        case StatementKind::Insert: {
            const auto& insert = statement.insert;
            Table& table = catalog.get_table(insert.table_name);
            const Schema& schema = table.schema();

            std::vector<std::size_t> target_columns;
            target_columns.reserve(
                insert.columns.empty()
                    ? schema.column_count()
                    : insert.columns.size());

            if (insert.columns.empty()) {
                for (std::size_t i = 0; i < schema.column_count(); ++i) {
                    target_columns.push_back(i);
                }
            } else {
                for (const auto& name : insert.columns) {
                    const auto index = schema.find_column(name);
                    if (!index.has_value()) {
                        throw std::invalid_argument(
                            "Unknown INSERT column: " + name);
                    }

                    for (const auto existing : target_columns) {
                        if (existing == *index) {
                            throw std::invalid_argument(
                                "Duplicate INSERT column: " + name);
                        }
                    }

                    target_columns.push_back(*index);
                }
            }

            // Validate and convert every row before modifying the table.
            std::vector<std::vector<Value>> pending_rows;
            pending_rows.reserve(insert.rows.size());

            for (const auto& literals : insert.rows) {
                pending_rows.push_back(
                    make_insert_row(literals, target_columns, schema));
            }

            for (auto& row : pending_rows) {
                table.append_row(std::move(row));
            }

            result.affected_rows = pending_rows.size();
            result.message = "Rows inserted: " +
                             std::to_string(result.affected_rows);
            return result;
        }

        case StatementKind::Select: {
            const auto& table_name = statement.select.table;
            const Table& table = catalog.get_table(table_name);

            if (!statement.select.join_table.empty()) {
                const auto& right_table =
                    catalog.get_table(statement.select.join_table);
                result.query_result = SqlExecutor::execute(
                    sql,
                    table_name,
                    table,
                    statement.select.join_table,
                    right_table);
            } else {
                result.query_result = SqlExecutor::execute(
                    sql, table_name, table);
            }
            result.message = "SELECT completed";
            return result;
        }
    }

    throw std::invalid_argument("Unsupported SQL statement kind");
}

}  // namespace atlasdb
