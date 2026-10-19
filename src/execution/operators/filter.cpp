#include "atlasdb/execution/operators/filter.hpp"

#include <stdexcept>
#include <utility>

namespace atlasdb {

Filter::Filter(
    std::unique_ptr<ExecutionOperator> input,
    Predicate predicate)
    : input_(std::move(input)),
      predicate_(std::move(predicate))
{
    if (!input_) {
        throw std::invalid_argument("Filter input must not be null");
    }

    if (!predicate_) {
        throw std::invalid_argument("Filter predicate must not be empty");
    }
}

Filter::Filter(
    std::unique_ptr<ExecutionOperator> input,
    std::unique_ptr<Expression> predicate,
    ExpressionTag)
    : input_(std::move(input)),
      expression_(std::move(predicate))
{
    if (!input_) {
        throw std::invalid_argument("Filter input must not be null");
    }

    if (!expression_) {
        throw std::invalid_argument("Filter expression must not be null");
    }
}

auto Filter::with_expression(
    std::unique_ptr<ExecutionOperator> input,
    std::unique_ptr<Expression> predicate) -> Filter
{
    return Filter(
        std::move(input),
        std::move(predicate),
        ExpressionTag{});
}

auto Filter::schema() const noexcept -> const Schema&
{
    return input_->schema();
}

auto Filter::next(std::vector<Value>& output_row) -> bool
{
    std::vector<Value> candidate;

    while (input_->next(candidate)) {
        if (expression_) {
            const Value result = expression_->evaluate(candidate);

            // SQL WHERE excludes NULL results.
            if (result.is_null()) {
                continue;
            }

            if (!result.is_compatible_with(DataType::Boolean)) {
                throw std::runtime_error(
                    "Filter expression must evaluate to a boolean");
            }

            if (!result.as<bool>()) {
                continue;
            }
        } else if (!predicate_(candidate)) {
            continue;
        }

        output_row = std::move(candidate);
        return true;
    }

    output_row.clear();
    return false;
}

}  // namespace atlasdb
