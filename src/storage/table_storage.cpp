#include "atlasdb/storage/table_storage.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace atlasdb {
namespace {

constexpr std::size_t kMaxColumns = 10000;
constexpr std::size_t kMaxRows = 10000000;
constexpr std::uintmax_t kMaxFileBytes = 512ULL * 1024ULL * 1024ULL;

auto hex_encode(std::string_view input) -> std::string {
    constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(input.size() * 2);

    for (const unsigned char byte : input) {
        output.push_back(digits[byte >> 4]);
        output.push_back(digits[byte & 0x0F]);
    }

    return output;
}

auto hex_decode(std::string_view input) -> std::string {
    if (input.size() % 2 != 0) {
        throw std::runtime_error("Invalid hex string in table file");
    }

    auto nibble = [](char ch) -> unsigned int {
        if (ch >= '0' && ch <= '9') {
            return static_cast<unsigned int>(ch - '0');
        }
        if (ch >= 'a' && ch <= 'f') {
            return static_cast<unsigned int>(ch - 'a' + 10);
        }
        if (ch >= 'A' && ch <= 'F') {
            return static_cast<unsigned int>(ch - 'A' + 10);
        }
        throw std::runtime_error("Invalid hex digit in table file");
    };

    std::string output;
    output.reserve(input.size() / 2);

    for (std::size_t i = 0; i < input.size(); i += 2) {
        const auto byte = (nibble(input[i]) << 4) | nibble(input[i + 1]);
        output.push_back(static_cast<char>(byte));
    }

    return output;
}

auto read_token(std::istream& input, const char* description) -> std::string {
    std::string token;
    if (!(input >> token)) {
        throw std::runtime_error(
            std::string("Unexpected end of table file while reading ") +
            description);
    }
    return token;
}

void expect_token(
    std::istream& input,
    const char* expected,
    const char* description) {
    const auto actual = read_token(input, description);
    if (actual != expected) {
        throw std::runtime_error(
            std::string("Invalid table file: expected ") + expected +
            " while reading " + description);
    }
}

auto parse_size(
    std::istream& input,
    const char* description,
    std::size_t maximum) -> std::size_t {
    const auto token = read_token(input, description);
    std::size_t value = 0;
    const auto result = std::from_chars(
        token.data(), token.data() + token.size(), value);

    if (result.ec != std::errc{} ||
        result.ptr != token.data() + token.size() ||
        value > maximum) {
        throw std::runtime_error(
            std::string("Invalid or excessive ") + description +
            " in table file");
    }

    return value;
}

auto parse_value(const std::string& token) -> Value {
    if (token == "N") {
        return Value::null();
    }

    if (token.size() < 2 || token[1] != ':') {
        throw std::runtime_error("Invalid value encoding in table file");
    }

    const std::string_view payload(token.data() + 2, token.size() - 2);

    switch (token[0]) {
    case 'I': {
        std::int64_t value = 0;
        const auto result = std::from_chars(
            payload.data(), payload.data() + payload.size(), value);
        if (result.ec != std::errc{} ||
            result.ptr != payload.data() + payload.size()) {
            throw std::runtime_error("Invalid integer in table file");
        }
        return Value(value);
    }
    case 'D': {
        const std::string text(payload);
        char* end = nullptr;
        const double value = std::strtod(text.c_str(), &end);
        if (text.empty() || end != text.c_str() + text.size() ||
            !std::isfinite(value)) {
            throw std::runtime_error("Invalid double in table file");
        }
        return Value(value);
    }
    case 'B':
        if (payload == "0") {
            return Value(false);
        }
        if (payload == "1") {
            return Value(true);
        }
        throw std::runtime_error("Invalid boolean in table file");
    case 'S':
        return Value(hex_decode(payload));
    default:
        throw std::runtime_error("Unknown value type in table file");
    }
}

auto encode_value(const Value& value) -> std::string {
    if (value.is_null()) {
        return "N";
    }

    const auto& storage = value.storage();

    if (const auto* integer = std::get_if<std::int64_t>(&storage)) {
        return "I:" + std::to_string(*integer);
    }

    if (const auto* number = std::get_if<double>(&storage)) {
        if (!std::isfinite(*number)) {
            throw std::runtime_error(
                "Cannot persist a non-finite double value");
        }
        std::ostringstream output;
        output << "D:" << std::hexfloat << *number;
        return output.str();
    }

    if (const auto* boolean = std::get_if<bool>(&storage)) {
        return *boolean ? "B:1" : "B:0";
    }

    return "S:" + hex_encode(std::get<std::string>(storage));
}

auto encode_type(DataType type) -> int {
    switch (type) {
    case DataType::Int64: return 0;
    case DataType::Double: return 1;
    case DataType::Boolean: return 2;
    case DataType::String: return 3;
    }
    throw std::runtime_error("Unsupported column data type");
}

auto decode_type(std::size_t value) -> DataType {
    switch (value) {
    case 0: return DataType::Int64;
    case 1: return DataType::Double;
    case 2: return DataType::Boolean;
    case 3: return DataType::String;
    default:
        throw std::runtime_error("Unknown column type in table file");
    }
}

}  // namespace

void TableStorage::save(
    const Table& table,
    const std::filesystem::path& path) {
    if (path.empty()) {
        throw std::invalid_argument("Table file path cannot be empty");
    }

    const auto temporary = std::filesystem::path(path.string() + ".tmp");
    std::error_code error;

    if (!path.parent_path().empty() &&
        !std::filesystem::exists(path.parent_path(), error)) {
        throw std::runtime_error("Table file directory does not exist");
    }

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Could not open temporary table file");
        }

        output << "ATLASDB_TABLE 1\n";
        output << "COLUMNS " << table.schema().column_count() << '\n';

        for (const auto& column : table.schema().columns()) {
            output << "COLUMN " << encode_type(column.type) << ' '
                   << hex_encode(column.name) << '\n';
        }

        output << "ROWS " << table.row_count() << '\n';

        for (std::size_t row = 0; row < table.row_count(); ++row) {
            output << "ROW";
            for (std::size_t col = 0;
                 col < table.schema().column_count(); ++col) {
                output << ' ' << encode_value(table.value_at(row, col));
            }
            output << '\n';
        }

        output.flush();
        if (!output) {
            output.close();
            std::filesystem::remove(temporary, error);
            throw std::runtime_error("Failed while writing table file");
        }
    }

    // Windows rename does not replace an existing destination.
    // Keep the temporary file intact if replacement fails.
    error.clear();
    if (std::filesystem::exists(path, error)) {
        const auto backup = std::filesystem::path(path.string() + ".bak");
        error.clear();
        std::filesystem::remove(backup, error);
        error.clear();

        std::filesystem::rename(path, backup, error);
        if (error) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not back up existing table file");
        }

        std::filesystem::rename(temporary, path, error);
        if (error) {
            std::error_code restore_error;
            std::filesystem::rename(backup, path, restore_error);
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not replace table file");
        }

        std::filesystem::remove(backup, error);
    } else {
        error.clear();
        std::filesystem::rename(temporary, path, error);
        if (error) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not finalize table file");
        }
    }
}

auto TableStorage::load(const std::filesystem::path& path) -> Table {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::runtime_error("Could not access table file");
    }
    if (size > kMaxFileBytes) {
        throw std::runtime_error("Table file exceeds the maximum supported size");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open table file");
    }

    expect_token(input, "ATLASDB_TABLE", "file signature");
    const auto version = parse_size(input, "file version", 100);
    if (version != 1) {
        throw std::runtime_error("Unsupported table file version");
    }

    expect_token(input, "COLUMNS", "column count");
    const auto column_count = parse_size(input, "column count", kMaxColumns);

    std::vector<Column> columns;
    columns.reserve(column_count);

    for (std::size_t i = 0; i < column_count; ++i) {
        expect_token(input, "COLUMN", "column definition");
        const auto type_value = parse_size(input, "column type", 3);
        const auto name = hex_decode(read_token(input, "column name"));
        columns.push_back(Column{name, decode_type(type_value)});
    }

    Schema schema(std::move(columns));
    expect_token(input, "ROWS", "row count");
    const auto row_count = parse_size(input, "row count", kMaxRows);

    Table table(std::move(schema));

    for (std::size_t row = 0; row < row_count; ++row) {
        expect_token(input, "ROW", "row data");

        std::vector<Value> values;
        values.reserve(column_count);

        for (std::size_t col = 0; col < column_count; ++col) {
            values.push_back(parse_value(read_token(input, "cell value")));
        }

        table.append_row(std::move(values));
    }

    std::string trailing;
    if (input >> trailing) {
        throw std::runtime_error("Unexpected trailing data in table file");
    }

    if (!input.eof()) {
        throw std::runtime_error("Failed while reading table file");
    }

    return table;
}

}  // namespace atlasdb