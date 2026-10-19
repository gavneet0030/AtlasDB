#include "atlasdb/execution/operators/table_scan.hpp"

namespace atlasdb {

TableScan::TableScan(const Table& table) noexcept
    : table_(table)
{
}

auto TableScan::schema() const noexcept -> const Schema&
{
    return table_.schema();
}

auto TableScan::next(std::vector<Value>& output_row) -> bool
{
    if (next_row_ >= table_.row_count()) {
        output_row.clear();
        return false;
    }

    output_row.clear();
    output_row.reserve(table_.schema().column_count());

    for (std::size_t column_index = 0;
         column_index < table_.schema().column_count();
         ++column_index) {
        output_row.push_back(table_.value_at(next_row_, column_index));
    }

    ++next_row_;
    return true;
}

}  // namespace atlasdb
