#pragma once

#include "atlasdb/execution/operators/execution_operator.hpp"
#include "atlasdb/execution/expressions/expression.hpp"
#include "atlasdb/storage/table.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace atlasdb {

class NestedLoopJoin final : public ExecutionOperator {
public:
    NestedLoopJoin(
        const Table& left,
        std::string left_name,
        const Table& right,
        std::string right_name,
        std::unique_ptr<Expression> predicate)
        : left_(left),
          right_(right),
          predicate_(std::move(predicate)),
          output_schema_(make_schema(
              left, left_name, right, right_name)) {
        if (!predicate_) {
            throw std::invalid_argument(
                "JOIN predicate must not be null");
        }
    }

    [[nodiscard]] auto schema() const noexcept
        -> const Schema& override {
        return output_schema_;
    }

    auto next(std::vector<Value>& output_row) -> bool override {
        while (left_row_ < left_.row_count()) {
            while (right_row_ < right_.row_count()) {
                std::vector<Value> candidate;
                candidate.reserve(output_schema_.column_count());

                for (std::size_t col = 0;
                     col < left_.schema().column_count(); ++col) {
                    candidate.push_back(left_.value_at(left_row_, col));
                }

                for (std::size_t col = 0;
                     col < right_.schema().column_count(); ++col) {
                    candidate.push_back(right_.value_at(right_row_, col));
                }

                ++right_row_;

                const Value result = predicate_->evaluate(candidate);
                if (result.is_null()) {
                    continue;
                }
                if (!result.is_compatible_with(DataType::Boolean)) {
                    throw std::runtime_error(
                        "JOIN condition must evaluate to a boolean");
                }
                if (!result.as<bool>()) {
                    continue;
                }

                output_row = std::move(candidate);
                return true;
            }

            ++left_row_;
            right_row_ = 0;
        }

        output_row.clear();
        return false;
    }

private:
    static auto make_schema(
        const Table& left,
        const std::string& left_name,
        const Table& right,
        const std::string& right_name) -> Schema {
        std::vector<Column> columns;
        columns.reserve(
            left.schema().column_count() +
            right.schema().column_count());

        for (const auto& column : left.schema().columns()) {
            columns.push_back({
                left_name + "." + column.name,
                column.type
            });
        }

        for (const auto& column : right.schema().columns()) {
            columns.push_back({
                right_name + "." + column.name,
                column.type
            });
        }

        return Schema(std::move(columns));
    }

    const Table& left_;
    const Table& right_;
    std::unique_ptr<Expression> predicate_;
    Schema output_schema_;
    std::size_t left_row_{0};
    std::size_t right_row_{0};
};

}  // namespace atlasdb