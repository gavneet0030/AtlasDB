#include "atlasdb/catalog/database_catalog.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace atlasdb {
namespace {

Schema make_schema() {
    return Schema({
        Column{"id", DataType::Int64},
        Column{"name", DataType::String}
    });
}

TEST(DatabaseCatalogTest, CreatesAndFindsTable) {
    DatabaseCatalog catalog;
    catalog.create_table("users", make_schema());

    EXPECT_TRUE(catalog.contains_table("users"));
    EXPECT_EQ(catalog.table_count(), 1U);
}

TEST(DatabaseCatalogTest, LooksUpTableNamesCaseInsensitively) {
    DatabaseCatalog catalog;
    catalog.create_table("Users", make_schema());

    EXPECT_TRUE(catalog.contains_table("USERS"));
    EXPECT_NO_THROW(catalog.get_table("users"));
}

TEST(DatabaseCatalogTest, RejectsDuplicateNamesRegardlessOfCase) {
    DatabaseCatalog catalog;
    catalog.create_table("users", make_schema());

    EXPECT_THROW(catalog.create_table("USERS", make_schema()),
                 std::invalid_argument);
}

TEST(DatabaseCatalogTest, RejectsEmptyTableName) {
    DatabaseCatalog catalog;

    EXPECT_THROW(catalog.create_table("", make_schema()),
                 std::invalid_argument);
}

TEST(DatabaseCatalogTest, RejectsLookupOfMissingTable) {
    DatabaseCatalog catalog;

    EXPECT_THROW(catalog.get_table("missing"), std::out_of_range);
}

TEST(DatabaseCatalogTest, ManagesMultipleIndependentTables) {
    DatabaseCatalog catalog;
    catalog.create_table("users", make_schema());
    catalog.create_table("orders", make_schema());

    EXPECT_EQ(catalog.table_count(), 2U);
    EXPECT_EQ(catalog.table_names(),
              (std::vector<std::string>{"orders", "users"}));
}

TEST(DatabaseCatalogTest, ReturnsMutableTableReference) {
    DatabaseCatalog catalog;
    Table& table = catalog.create_table("users", make_schema());

    EXPECT_EQ(&table, &catalog.get_table("users"));
}

}  // namespace
}  // namespace atlasdb
