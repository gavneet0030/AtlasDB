#include "atlasdb/execution/query_planner.hpp"

#include "atlasdb/execution/expressions/column_reference_expression.hpp"
#include "atlasdb/execution/expressions/comparison_expression.hpp"
#include "atlasdb/execution/expressions/literal_expression.hpp"
#include "atlasdb/execution/expressions/logical_expression.hpp"
#include "atlasdb/execution/operators/aggregation.hpp"
#include "atlasdb/execution/operators/filter.hpp"
#include "atlasdb/execution/operators/nested_loop_join.hpp"
#include "atlasdb/execution/operators/projection.hpp"
#include "atlasdb/execution/operators/sort.hpp"
#include "atlasdb/execution/operators/table_scan.hpp"

#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace atlasdb {
namespace {

auto normalized(std::string_view text) -> std::string {
    std::string result(text);

    for (char& ch : result) {
        ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
    }

    return result;
}

auto resolve_column(
    const Schema& schema,
    std::string_view name
) -> std::size_t {
    const auto requested = normalized(name);

    for (std::size_t i = 0; i < schema.column_count(); ++i) {
        if (normalized(schema.column_at(i).name) == requested) {
            return i;
        }
    }

    // In a joined schema, allow an unqualified name only when unique.
    if (name.find(".") == std::string_view::npos) {
        std::optional<std::size_t> match;
        for (std::size_t i = 0; i < schema.column_count(); ++i) {
            const auto& candidate = schema.column_at(i).name;
            const auto dot = candidate.find('.');
            if (dot == std::string::npos) {
                continue;
            }
            if (normalized(std::string_view(candidate).substr(dot + 1)) ==
                requested) {
                if (match.has_value()) {
                    throw std::invalid_argument(
                        "Ambiguous column in SQL query: " +
                        std::string(name));
                }
                match = i;
            }
        }
        if (match.has_value()) {
            return *match;
        }
    }

    throw std::invalid_argument(
        "Unknown column in SQL query: " + std::string(name));
}

auto make_literal(const ParsedExpression& node)
    -> std::unique_ptr<Expression> {
    switch (node.literal_kind) {
        case TokenKind::IntegerLiteral: {
            std::size_t consumed = 0;
            const auto parsed = std::stoll(node.value, &consumed);

            if (consumed != node.value.size()) {
                throw std::invalid_argument("Invalid integer literal");
            }

            return std::make_unique<LiteralExpression>(
                Value(static_cast<std::int64_t>(parsed)));
        }

        case TokenKind::FloatingLiteral: {
            std::size_t consumed = 0;
            const double parsed = std::stod(node.value, &consumed);

            if (consumed != node.value.size()) {
                throw std::invalid_argument("Invalid floating-point literal");
            }

            return std::make_unique<LiteralExpression>(Value(parsed));
        }

        case TokenKind::StringLiteral:
            return std::make_unique<LiteralExpression>(Value(node.value));

        case TokenKind::True:
            return std::make_unique<LiteralExpression>(Value(true));

        case TokenKind::False:
            return std::make_unique<LiteralExpression>(Value(false));

        case TokenKind::Null:
            return std::make_unique<LiteralExpression>(Value::null());

        default:
            throw std::invalid_argument(
                "Unsupported literal kind in SQL query");
    }
}

auto build_expression(
    const ParsedExpression& node,
    const Schema& schema
) -> std::unique_ptr<Expression> {
    switch (node.kind) {
        case ExpressionKind::Literal:
            return make_literal(node);

        case ExpressionKind::Column:
            return std::make_unique<ColumnReferenceExpression>(
                resolve_column(schema, node.value));

        case ExpressionKind::Equal:
        case ExpressionKind::NotEqual:
        case ExpressionKind::Less:
        case ExpressionKind::LessEqual:
        case ExpressionKind::Greater:
        case ExpressionKind::GreaterEqual: {
            if (!node.left || !node.right) {
                throw std::invalid_argument(
                    "Comparison expression is missing an operand");
            }

            ComparisonOperator op = ComparisonOperator::Equal;

            switch (node.kind) {
                case ExpressionKind::Equal:
                    op = ComparisonOperator::Equal;
                    break;
                case ExpressionKind::NotEqual:
                    op = ComparisonOperator::NotEqual;
                    break;
                case ExpressionKind::Less:
            op = ComparisonOperator::LessThan;
            break;
        case ExpressionKind::LessEqual:
            op = ComparisonOperator::LessThanOrEqual;
            break;
        case ExpressionKind::Greater:
            op = ComparisonOperator::GreaterThan;
            break;
        case ExpressionKind::GreaterEqual:
            op = ComparisonOperator::GreaterThanOrEqual;
            break;
                default:
                    break;
            }

            return std::make_unique<ComparisonExpression>(
                op,
                build_expression(*node.left, schema),
                build_expression(*node.right, schema));
        }

        case ExpressionKind::And:
        case ExpressionKind::Or: {
            if (!node.left || !node.right) {
                throw std::invalid_argument(
                    "Logical expression is missing an operand");
            }

            const auto op = node.kind == ExpressionKind::And
                ? LogicalOperator::And
                : LogicalOperator::Or;

            return std::make_unique<LogicalExpression>(
                op,
                build_expression(*node.left, schema),
                build_expression(*node.right, schema));
        }

        case ExpressionKind::Not:
            if (!node.left) {
                throw std::invalid_argument(
                    "NOT expression is missing its operand");
            }

            return std::make_unique<LogicalExpression>(
                LogicalOperator::Not,
                build_expression(*node.left, schema));
    }

    throw std::invalid_argument("Unsupported SQL expression");
}

}  // namespace

auto QueryPlanner::plan(
    const SelectQuery& query,
    std::string_view actual_table_name,
    const Table& table
) -> std::unique_ptr<ExecutionOperator> {
    if (normalized(query.table) != normalized(actual_table_name)) {
        throw std::invalid_argument(
            "SQL table name does not match the supplied table");
    }

    if (!query.select_all && query.columns.empty() &&
        query.aggregates.empty()) {
        throw std::invalid_argument(
            "SELECT must contain at least one column, aggregate, or *");
    }

    if (!query.group_by.empty() && query.aggregates.empty()) {
        throw std::invalid_argument(
            "GROUP BY requires an aggregate function");
    }

    std::unique_ptr<ExecutionOperator> plan =
        std::make_unique<TableScan>(table);

    if (query.where) {
        auto predicate = build_expression(*query.where, table.schema());

        plan = std::make_unique<Filter>(
            Filter::with_expression(std::move(plan), std::move(predicate)));
    }

    if (!query.aggregates.empty()) {
        if (!query.join_table.empty()) {
            throw std::invalid_argument(
                "Aggregate queries with JOIN are not supported yet");
        }


        std::vector<std::size_t> group_indices;
        group_indices.reserve(query.group_by.size());

        if (query.columns.size() != query.group_by.size()) {
            throw std::invalid_argument(
                "Selected ordinary columns must match GROUP BY columns");
        }

        for (std::size_t i = 0; i < query.group_by.size(); ++i) {
            if (normalized(query.columns[i]) !=
                normalized(query.group_by[i])) {
                throw std::invalid_argument(
                    "Selected ordinary columns must match GROUP BY columns "
                    "in the same order");
            }

            group_indices.push_back(
                resolve_column(table.schema(), query.group_by[i]));
        }

        std::vector<AggregateSpec> specs;
        specs.reserve(query.aggregates.size());

        for (const auto& selection : query.aggregates) {
            const auto function = normalized(selection.function);
            AggregateFunction aggregate_function;

            if (function == "count") {
                aggregate_function = AggregateFunction::Count;
            } else if (function == "sum") {
                aggregate_function = AggregateFunction::Sum;
            } else if (function == "min") {
                aggregate_function = AggregateFunction::Min;
            } else if (function == "max") {
                aggregate_function = AggregateFunction::Max;
            } else if (function == "avg") {
                aggregate_function = AggregateFunction::Avg;
            } else {
                throw std::invalid_argument(
                    "Unsupported SQL aggregate function: " +
                    selection.function);
            }

            if (selection.star_argument) {
                if (aggregate_function != AggregateFunction::Count) {
                    throw std::invalid_argument(
                        "Only COUNT supports * as its argument");
                }
                specs.push_back({aggregate_function, std::nullopt});
            } else {
                const auto index =
                    resolve_column(table.schema(), selection.column);
                specs.push_back({aggregate_function, index});
            }
        }

        plan = std::make_unique<Aggregation>(
            std::move(plan), std::move(group_indices), std::move(specs));

        if (!query.order_by.empty()) {
            std::vector<SortKey> keys;
            keys.reserve(query.order_by.size());

            for (const auto& item : query.order_by) {
                std::size_t sort_column_index = 0;

                if (item.is_aggregate) {
                    bool found = false;

                    for (std::size_t i = 0; i < query.aggregates.size(); ++i) {
                        const auto& selected = query.aggregates[i];

                        const bool same_function =
                            normalized(selected.function) ==
                            normalized(item.aggregate_function);
                        const bool same_argument =
                            selected.star_argument == item.aggregate_star_argument &&
                            (selected.star_argument ||
                             normalized(selected.column) ==
                                 normalized(item.aggregate_column));

                        if (same_function && same_argument) {
                            sort_column_index = query.group_by.size() + i;
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        throw std::invalid_argument(
                            "ORDER BY aggregate must also appear in SELECT");
                    }
                } else {
                    sort_column_index =
                        resolve_column(plan->schema(), item.column);
                }

                keys.push_back({
                    sort_column_index,
                    item.ascending
                        ? SortDirection::Ascending
                        : SortDirection::Descending,
                    NullPlacement::Last
                });
            }

            plan = std::make_unique<Sort>(
                std::move(plan), std::move(keys));
        }

        return plan;
    }

    if (!query.order_by.empty()) {
        std::vector<SortKey> keys;
        keys.reserve(query.order_by.size());

        for (const auto& item : query.order_by) {
            keys.push_back({
                resolve_column(table.schema(), item.column),
                item.ascending
                    ? SortDirection::Ascending
                    : SortDirection::Descending,
                NullPlacement::Last
            });
        }

        plan = std::make_unique<Sort>(
            std::move(plan), std::move(keys));
    }

    // Sort before projection so ORDER BY can reference unselected columns.
    if (!query.select_all) {
        std::vector<std::size_t> indices;
        indices.reserve(query.columns.size());

        for (const auto& name : query.columns) {
            indices.push_back(resolve_column(table.schema(), name));
        }

        plan = std::make_unique<Projection>(
            std::move(plan), std::move(indices));
    }

    return plan;
}

auto QueryPlanner::plan(
    const SelectQuery& query,
    std::string_view actual_left_name,
    const Table& left,
    std::string_view actual_right_name,
    const Table& right
) -> std::unique_ptr<ExecutionOperator> {
    if (!query.aggregates.empty()) {
        throw std::invalid_argument(
            "Aggregate queries with JOIN are not supported yet");
    }

    if (normalized(query.table) != normalized(actual_left_name)) {
        throw std::invalid_argument(
            "SQL left table name does not match the supplied table");
    }
    if (normalized(query.join_table) != normalized(actual_right_name)) {
        throw std::invalid_argument(
            "SQL JOIN table name does not match the supplied table");
    }
    if (!query.join_condition) {
        throw std::invalid_argument("JOIN requires an ON condition");
    }
    if (!query.select_all && query.columns.empty()) {
        throw std::invalid_argument(
            "SELECT must contain at least one column or *");
    }

    // Resolve ON against the combined, table-qualified schema.
    std::vector<Column> combined_columns;
    combined_columns.reserve(
        left.schema().column_count() + right.schema().column_count());

    for (const auto& column : left.schema().columns()) {
        combined_columns.push_back({
            std::string(actual_left_name) + "." + column.name,
            column.type
        });
    }
    for (const auto& column : right.schema().columns()) {
        combined_columns.push_back({
            std::string(actual_right_name) + "." + column.name,
            column.type
        });
    }
    const Schema combined_schema(std::move(combined_columns));

    auto on_expression =
        build_expression(*query.join_condition, combined_schema);

    std::unique_ptr<ExecutionOperator> plan =
        std::make_unique<NestedLoopJoin>(
            left, std::string(actual_left_name),
            right, std::string(actual_right_name),
            std::move(on_expression));

    if (query.where) {
        auto predicate = build_expression(*query.where, combined_schema);
        plan = std::make_unique<Filter>(
            Filter::with_expression(std::move(plan), std::move(predicate)));
    }

    if (!query.order_by.empty()) {
        std::vector<SortKey> keys;
        keys.reserve(query.order_by.size());
        for (const auto& item : query.order_by) {
            keys.push_back({
                resolve_column(combined_schema, item.column),
                item.ascending
                    ? SortDirection::Ascending
                    : SortDirection::Descending,
                NullPlacement::Last
            });
        }
        plan = std::make_unique<Sort>(std::move(plan), std::move(keys));
    }

    if (!query.select_all) {
        std::vector<std::size_t> indices;
        indices.reserve(query.columns.size());
        for (const auto& name : query.columns) {
            indices.push_back(resolve_column(combined_schema, name));
        }
        plan = std::make_unique<Projection>(
            std::move(plan), std::move(indices));
    }

    return plan;
}

}  // namespace atlasdb
