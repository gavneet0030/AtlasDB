#pragma once

#include "atlasdb/execution/operators/execution_operator.hpp"
#include "atlasdb/parser/parser.hpp"
#include "atlasdb/storage/table.hpp"

#include <memory>
#include <string_view>

namespace atlasdb {

class QueryPlanner {
public:
    [[nodiscard]] static auto plan(
        const SelectQuery& query,
        std::string_view actual_table_name,
        const Table& table
    ) -> std::unique_ptr<ExecutionOperator>;

    [[nodiscard]] static auto plan(
        const SelectQuery& query,
        std::string_view actual_left_name,
        const Table& left,
        std::string_view actual_right_name,
        const Table& right
    ) -> std::unique_ptr<ExecutionOperator>;
};

}  // namespace atlasdb
