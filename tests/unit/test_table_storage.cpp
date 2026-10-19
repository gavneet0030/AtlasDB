#include "atlasdb/storage/table_storage.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace atlasdb {
namespace {

auto test_path(const char* suffix) -> std::filesystem::path {
    return std::filesystem::temp_directory_path() /
        (std::string("atlasdb_storage_test_") + suffix + ".atdb");
}

struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
        std::error_code error;
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.string() + ".tmp", error);
        std::filesystem::remove(path.string() + ".bak", error);
    }
};

TEST(TableStorageTest, RoundTripsAllValueTypesAndSpecialStrings) {
    const auto path = test_path("roundtrip");
    Cleanup cleanup{path};

    Schema schema(std::vector<Column>{
        {"id", DataType::Int64},
        {"score", DataType::Double},
        {"active", DataType::Boolean},
        {"name", DataType::String},
        {"optional", DataType::String}
    });

    Table original(std::move(schema));
    original.append_row({
        Value(std::int64_t{-42}),
        Value(3.141592653589793),
        Value(true),
        Value(std::string("spaces, quotes \" and\nnewline")),
        Value::null()
    });
    original.append_row({
        Value(std::int64_t{0}),
        Value(-0.125),
        Value(false),
        Value(std::string("O'Brien")),
        Value(std::string(""))
    });

    TableStorage::save(original, path);
    const auto loaded = TableStorage::load(path);

    ASSERT_EQ(loaded.row_count(), 2U);
    ASSERT_EQ(loaded.schema().column_count(), 5U);
    EXPECT_EQ(loaded.schema().columns()[3].name, "name");
    EXPECT_EQ(loaded.value_at(0, 0).as<std::int64_t>(), -42);
    EXPECT_DOUBLE_EQ(
        loaded.value_at(0, 1).as<double>(), 3.141592653589793);
    EXPECT_TRUE(loaded.value_at(0, 2).as<bool>());
    EXPECT_EQ(
        loaded.value_at(0, 3).as<std::string>(),
        "spaces, quotes \" and\nnewline");
    EXPECT_TRUE(loaded.value_at(0, 4).is_null());
    EXPECT_FALSE(loaded.value_at(1, 2).as<bool>());
    EXPECT_EQ(loaded.value_at(1, 3).as<std::string>(), "O'Brien");
    EXPECT_EQ(loaded.value_at(1, 4).as<std::string>(), "");
}

TEST(TableStorageTest, RoundTripsEmptyTable) {
    const auto path = test_path("empty");
    Cleanup cleanup{path};

    Table original(Schema(std::vector<Column>{
        {"id", DataType::Int64},
        {"label", DataType::String}
    }));

    TableStorage::save(original, path);
    const auto loaded = TableStorage::load(path);

    EXPECT_TRUE(loaded.empty());
    EXPECT_EQ(loaded.row_count(), 0U);
    EXPECT_EQ(loaded.schema().column_count(), 2U);
}

TEST(TableStorageTest, ReplacesExistingFile) {
    const auto path = test_path("replace");
    Cleanup cleanup{path};

    Table first(Schema(std::vector<Column>{{"id", DataType::Int64}}));
    first.append_row({Value(1)});
    TableStorage::save(first, path);

    Table second(Schema(std::vector<Column>{{"id", DataType::Int64}}));
    second.append_row({Value(2)});
    second.append_row({Value(3)});
    TableStorage::save(second, path);

    const auto loaded = TableStorage::load(path);
    ASSERT_EQ(loaded.row_count(), 2U);
    EXPECT_EQ(loaded.value_at(0, 0).as<std::int64_t>(), 2);
    EXPECT_EQ(loaded.value_at(1, 0).as<std::int64_t>(), 3);
}

TEST(TableStorageTest, RejectsInvalidSignature) {
    const auto path = test_path("bad_signature");
    Cleanup cleanup{path};

    {
        std::ofstream output(path);
        output << "NOT_ATLASDB\n";
    }

    EXPECT_THROW((void)TableStorage::load(path), std::runtime_error);
}

TEST(TableStorageTest, RejectsUnknownVersion) {
    const auto path = test_path("bad_version");
    Cleanup cleanup{path};

    {
        std::ofstream output(path);
        output << "ATLASDB_TABLE 999\n";
    }

    EXPECT_THROW((void)TableStorage::load(path), std::runtime_error);
}

TEST(TableStorageTest, RejectsMissingFile) {
    const auto path = test_path("missing");
    Cleanup cleanup{path};
    std::error_code error;
    std::filesystem::remove(path, error);

    EXPECT_THROW((void)TableStorage::load(path), std::runtime_error);
}

}  // namespace
}  // namespace atlasdb