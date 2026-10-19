#include "atlasdb/catalog/database_catalog.hpp"
#include "atlasdb/common/version.hpp"
#include "atlasdb/execution/database_sql_executor.hpp"
#include "atlasdb/storage/table_storage.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

auto make_demo_table() -> atlasdb::Table {
    atlasdb::Schema schema(std::vector<atlasdb::Column>{
        {"id", atlasdb::DataType::Int64},
        {"name", atlasdb::DataType::String},
        {"active", atlasdb::DataType::Boolean}
    });

    atlasdb::Table table(std::move(schema));
    table.append_row({
        atlasdb::Value(std::int64_t{1}),
        atlasdb::Value("Aman"),
        atlasdb::Value(true)
    });
    table.append_row({
        atlasdb::Value(std::int64_t{2}),
        atlasdb::Value("Priya"),
        atlasdb::Value(false)
    });
    table.append_row({
        atlasdb::Value(std::int64_t{3}),
        atlasdb::Value("Rahul"),
        atlasdb::Value(true)
    });
    table.append_row({
        atlasdb::Value(std::int64_t{4}),
        atlasdb::Value("O'Brien"),
        atlasdb::Value(true)
    });
    return table;
}

auto make_demo_catalog() -> atlasdb::DatabaseCatalog {
    auto demo = make_demo_table();
    atlasdb::DatabaseCatalog catalog;
    auto& users = catalog.create_table("users", demo.schema());

    for (std::size_t row = 0; row < demo.row_count(); ++row) {
        std::vector<atlasdb::Value> values;
        values.reserve(demo.schema().column_count());

        for (std::size_t col = 0; col < demo.schema().column_count(); ++col) {
            values.push_back(demo.value_at(row, col));
        }
        users.append_row(std::move(values));
    }

    return catalog;
}

auto make_catalog_from_file(const std::filesystem::path& path)
    -> atlasdb::DatabaseCatalog {
    auto table = atlasdb::TableStorage::load(path);
    atlasdb::DatabaseCatalog catalog;
    auto& users = catalog.create_table("users", table.schema());

    for (std::size_t row = 0; row < table.row_count(); ++row) {
        std::vector<atlasdb::Value> values;
        values.reserve(table.schema().column_count());

        for (std::size_t col = 0; col < table.schema().column_count(); ++col) {
            values.push_back(table.value_at(row, col));
        }
        users.append_row(std::move(values));
    }

    return catalog;
}

auto hex_encode_name(std::string_view text) -> std::string {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(text.size() * 2);
    for (unsigned char ch : text) {
        result.push_back(digits[ch >> 4]);
        result.push_back(digits[ch & 0x0F]);
    }
    return result;
}

auto hex_decode_name(std::string_view text) -> std::string {
    if (text.size() % 2 != 0) {
        throw std::runtime_error("Invalid database manifest name.");
    }

    auto nibble = [](char ch) -> unsigned int {
        if (ch >= '0' && ch <= '9') return static_cast<unsigned int>(ch - '0');
        if (ch >= 'a' && ch <= 'f') return static_cast<unsigned int>(ch - 'a' + 10);
        if (ch >= 'A' && ch <= 'F') return static_cast<unsigned int>(ch - 'A' + 10);
        throw std::runtime_error("Invalid hexadecimal database manifest name.");
    };

    std::string result;
    result.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        result.push_back(static_cast<char>((nibble(text[i]) << 4) |
                                           nibble(text[i + 1])));
    }
    return result;
}

auto database_manifest_path(const std::filesystem::path& directory)
    -> std::filesystem::path {
    return directory / "atlasdb.manifest";
}

auto database_table_path(
    const std::filesystem::path& directory,
    std::string_view name) -> std::filesystem::path {
    return directory / ("table_" + hex_encode_name(name) + ".adb");
}

auto load_database(const std::filesystem::path& directory)
    -> atlasdb::DatabaseCatalog {
    atlasdb::DatabaseCatalog catalog;
    const auto manifest = database_manifest_path(directory);

    if (!std::filesystem::exists(manifest)) {
        return catalog;
    }

    std::ifstream input(manifest);
    if (!input) {
        throw std::runtime_error("Could not open database manifest.");
    }

    std::string signature;
    unsigned int version = 0;
    std::string tables_label;
    std::size_t count = 0;

    if (!(input >> signature >> version) ||
        signature != "ATLASDB_DATABASE" || version != 1 ||
        !(input >> tables_label >> count) || tables_label != "TABLES" ||
        count > 10000) {
        throw std::runtime_error("Invalid or unsupported database manifest.");
    }

    for (std::size_t i = 0; i < count; ++i) {
        std::string encoded_name;
        if (!(input >> encoded_name)) {
            throw std::runtime_error("Incomplete database manifest.");
        }

        const auto name = hex_decode_name(encoded_name);
        auto table = atlasdb::TableStorage::load(
            database_table_path(directory, name));

        auto& destination = catalog.create_table(name, table.schema());
        for (std::size_t row = 0; row < table.row_count(); ++row) {
            std::vector<atlasdb::Value> values;
            values.reserve(table.schema().column_count());

            for (std::size_t col = 0;
                 col < table.schema().column_count(); ++col) {
                values.push_back(table.value_at(row, col));
            }
            destination.append_row(std::move(values));
        }
    }

    std::string trailing;
    if (input >> trailing) {
        throw std::runtime_error("Unexpected trailing data in database manifest.");
    }

    return catalog;
}

void save_database(
    const std::filesystem::path& directory,
    const atlasdb::DatabaseCatalog& catalog) {
    std::filesystem::create_directories(directory);

    const auto names = catalog.table_names();

    // Write table files before publishing the new manifest.
    for (const auto& name : names) {
        atlasdb::TableStorage::save(
            catalog.get_table(name), database_table_path(directory, name));
    }

    const auto manifest = database_manifest_path(directory);
    const auto temporary = std::filesystem::path(manifest.string() + ".tmp");
    const auto backup = std::filesystem::path(manifest.string() + ".bak");

    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Could not create database manifest.");
        }

        output << "ATLASDB_DATABASE 1\n";
        output << "TABLES " << names.size() << '\n';
        for (const auto& name : names) {
            output << hex_encode_name(name) << '\n';
        }
        output.flush();

        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error("Failed writing database manifest.");
        }
    }

    std::error_code error;
    if (std::filesystem::exists(manifest)) {
        std::filesystem::remove(backup, error);
        error.clear();
        std::filesystem::rename(manifest, backup, error);
        if (error) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not back up database manifest.");
        }

        std::filesystem::rename(temporary, manifest, error);
        if (error) {
            std::error_code restore_error;
            std::filesystem::rename(backup, manifest, restore_error);
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not replace database manifest.");
        }
        std::filesystem::remove(backup, error);
    } else {
        std::filesystem::rename(temporary, manifest, error);
        if (error) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not finalize database manifest.");
        }
    }
}
void print_result(const atlasdb::QueryResult& result) {
    const auto& columns = result.schema.columns();

    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i != 0) std::cout << " | ";
        std::cout << columns[i].name;
    }
    std::cout << '\n';

    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i != 0) std::cout << "-+-";
        std::cout << std::string(columns[i].name.size(), '-');
    }
    std::cout << '\n';

    for (const auto& row : result.rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i != 0) std::cout << " | ";
            std::cout << row[i].to_string();
        }
        std::cout << '\n';
    }

    std::cout << result.rows.size() << " row(s)\n";
}

auto split_statements(std::string_view sql) -> std::vector<std::string> {
    std::vector<std::string> statements;
    std::string current;
    bool in_string = false;

    for (std::size_t i = 0; i < sql.size(); ++i) {
        const char ch = sql[i];

        if (ch == '\'') {
            current.push_back(ch);

            if (in_string && i + 1 < sql.size() && sql[i + 1] == '\'') {
                current.push_back(sql[++i]);
                continue;
            }

            in_string = !in_string;
            continue;
        }

        if (ch == ';' && !in_string) {
            if (current.find_first_not_of(" \t\r\n") != std::string::npos) {
                statements.push_back(std::move(current));
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (current.find_first_not_of(" \t\r\n") != std::string::npos) {
        statements.push_back(std::move(current));
    }

    return statements;
}

auto execute_sql(
    std::string_view sql,
    atlasdb::DatabaseCatalog& catalog,
    const std::optional<std::filesystem::path>& persistence_path = std::nullopt,
    bool persist_entire_catalog = false)
    -> bool {
    const auto statements = split_statements(sql);

    if (statements.empty()) {
        std::cerr << "SQL error: no SQL statement provided.\n";
        return false;
    }

    bool all_succeeded = true;

    for (const auto& statement : statements) {
        try {
            const auto result =
                atlasdb::DatabaseSqlExecutor::execute(statement, catalog);

            if (result.query_result.has_value()) {
                print_result(*result.query_result);
            } else {
                std::cout << result.message << '\n';
            }

            if (persistence_path.has_value() &&
                result.kind != atlasdb::StatementKind::Select) {
                if (persist_entire_catalog) {
                    save_database(*persistence_path, catalog);
                } else if (catalog.contains_table("users")) {
                    atlasdb::TableStorage::save(
                        catalog.get_table("users"), *persistence_path);
                }
            }
        } catch (const std::exception& error) {
            std::cerr << "SQL error: " << error.what() << '\n';
            all_succeeded = false;
        }
    }

    return all_succeeded;
}

auto join_arguments(int argc, char** argv, int start) -> std::string {
    std::string result;

    for (int i = start; i < argc; ++i) {
        if (!result.empty()) result += ' ';
        result += argv[i];
    }

    return result;
}

auto run_shell(
    atlasdb::DatabaseCatalog& catalog,
    const std::optional<std::filesystem::path>& persistence_path = std::nullopt,
    bool persist_entire_catalog = false)
    -> int {
    std::cout
        << "AtlasDB interactive SQL shell\n"
        << "Tables currently loaded: ";

    const auto names = catalog.table_names();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) std::cout << ", ";
        std::cout << names[i];
    }

    std::cout << "\nEnter SQL statements, or type exit / quit to leave.\n"
              << "Multiple semicolon-separated statements are supported.\n\n";

    std::string line;
    while (true) {
        std::cout << "atlasdb> " << std::flush;

        if (!std::getline(std::cin, line)) break;

        const auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;

        const auto last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);

        if (line == "exit" || line == "quit") {
            std::cout << "Leaving AtlasDB shell.\n";
            break;
        }

        execute_sql(line, catalog, persistence_path, persist_entire_catalog);
        std::cout << '\n';
    }

    return 0;
}

void print_usage() {
    std::cout
        << "AtlasDB " << atlasdb::version() << '\n'
        << "Usage:\n"
        << "  atlasdb.exe --help\n"
        << "  atlasdb.exe --version\n"
        << "  atlasdb.exe --sql \"SELECT * FROM users;\"\n"
        << "  atlasdb.exe --shell\n"
        << "  atlasdb.exe --save-demo <file>\n"
        << "  atlasdb.exe --sql-file <file> \"<SQL statements>\"\n"
        << "  atlasdb.exe --shell-file <file>\n"
        << "  atlasdb.exe --database <directory> --sql \"<SQL statements>\"\n"
        << "  atlasdb.exe --database <directory> --shell\n\n"
        << "Database mode persists all catalog tables between runs.\n"
        << "The default catalog starts with the demo users table.\n"
        << "CREATE TABLE, INSERT and SELECT are supported.\n"
        << "Tables created during a process run are held in memory.\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--help") {
            print_usage();
            return 0;
        }
        if (argc > 1 && std::string_view(argv[1]) == "--version") {
            std::cout << "AtlasDB " << atlasdb::version() << '\n';
            return 0;
        }

        if (argc > 1 && std::string_view(argv[1]) == "--save-demo") {
            if (argc != 3) {
                std::cerr << "Usage: atlasdb.exe --save-demo <file>\n";
                return 2;
            }

            const auto table = make_demo_table();
            atlasdb::TableStorage::save(table, std::filesystem::path(argv[2]));
            std::cout << "Saved demo table 'users' with " << table.row_count()
                      << " row(s) to " << argv[2] << '\n';
            return 0;
        }

        if (argc > 1 && std::string_view(argv[1]) == "--sql-file") {
            if (argc < 4) {
                std::cerr
                    << "Usage: atlasdb.exe --sql-file <file> \"<SQL statements>\"\n";
                return 2;
            }

            auto catalog = make_catalog_from_file(std::filesystem::path(argv[2]));
            const auto sql = join_arguments(argc, argv, 3);
            return execute_sql(sql, catalog, std::filesystem::path(argv[2])) ? 0 : 1;
        }

        if (argc > 1 && std::string_view(argv[1]) == "--shell-file") {
            if (argc != 3) {
                std::cerr << "Usage: atlasdb.exe --shell-file <file>\n";
                return 2;
            }

            const std::filesystem::path path(argv[2]);
            auto catalog = make_catalog_from_file(path);
            return run_shell(catalog, path);
        }

        if (argc > 1 && std::string_view(argv[1]) == "--database") {
            if (argc < 4) {
                std::cerr
                    << "Usage: atlasdb.exe --database <directory> --sql \"<SQL>\"\n"
                    << "   or: atlasdb.exe --database <directory> --shell\n";
                return 2;
            }

            const std::filesystem::path directory(argv[2]);
            auto catalog = load_database(directory);
            const std::string_view mode(argv[3]);

            if (mode == "--shell") {
                if (argc != 4) {
                    std::cerr << "Usage: atlasdb.exe --database <directory> --shell\n";
                    return 2;
                }
                return run_shell(catalog, directory, true);
            }

            if (mode == "--sql") {
                if (argc < 5) {
                    std::cerr << "Missing SQL after --database <directory> --sql.\n";
                    return 2;
                }
                const auto sql = join_arguments(argc, argv, 4);
                return execute_sql(sql, catalog, directory, true) ? 0 : 1;
            }

            std::cerr << "Expected --sql or --shell after the database directory.\n";
            return 2;
        }

        if (argc > 1 && std::string_view(argv[1]) == "--sql") {
            if (argc < 3) {
                std::cerr << "Missing SQL statements after --sql.\n";
                return 2;
            }

            auto catalog = make_demo_catalog();
            return execute_sql(join_arguments(argc, argv, 2), catalog) ? 0 : 1;
        }

        if (argc > 1 && std::string_view(argv[1]) == "--shell") {
            auto catalog = make_demo_catalog();
            return run_shell(catalog);
        }

        print_usage();
        return argc == 1 ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "AtlasDB error: " << error.what() << '\n';
        return 1;
    }
}
