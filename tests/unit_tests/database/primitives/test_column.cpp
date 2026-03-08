#include <string>

#include <db_column.hpp>
#include <gtest/gtest.h>

using namespace demiplane::db;

TEST(ColumnTest, ConstructionWithNameAndTable) {
    const Column column{"test_field", "test_table"};

    EXPECT_EQ(column.name(), "test_field");
    EXPECT_EQ(column.table_name(), "test_table");
    EXPECT_TRUE(column.alias().empty());
}

TEST(ColumnTest, ConstructionWithNameOnly) {
    const Column column{"test_field"};

    EXPECT_EQ(column.name(), "test_field");
    EXPECT_TRUE(column.table_name().empty());
    EXPECT_TRUE(column.alias().empty());
}

TEST(ColumnTest, ColumnWithAlias) {
    const Column column{"test_field", "test_table"};
    const Column aliased = column.as("t");

    EXPECT_TRUE(column.alias().empty());
    EXPECT_EQ(aliased.alias(), "t");
    EXPECT_EQ(aliased.table_name(), "test_table");
    EXPECT_EQ(aliased.name(), "test_field");
}

TEST(ColumnTest, SetTable) {
    Column column{"test_field"};
    column.set_table("new_table");

    EXPECT_EQ(column.table_name(), "new_table");
}

TEST(ColumnTest, SetName) {
    Column column{"old_name"};
    column.set_name("new_name");

    EXPECT_EQ(column.name(), "new_name");
}

TEST(ColumnTest, AllColumnsWithTable) {
    const AllColumns all_cols{"users"};

    EXPECT_EQ(all_cols.table_name(), "users");
}

TEST(ColumnTest, AllColumnsDefault) {
    const AllColumns all_cols;

    EXPECT_TRUE(all_cols.table_name().empty());
}

TEST(ColumnTest, ColumnCreationHelper) {
    const auto column = col("test_field", "test_table");

    EXPECT_EQ(column.name(), "test_field");
    EXPECT_EQ(column.table_name(), "test_table");
}

TEST(ColumnTest, ColumnCreationHelperNameOnly) {
    const auto column = col("test_field");

    EXPECT_EQ(column.name(), "test_field");
    EXPECT_TRUE(column.table_name().empty());
}

TEST(ColumnTest, AllColumnsHelper) {
    const auto all_with_table = all("users");

    EXPECT_EQ(all_with_table.table_name(), "users");
}

TEST(ColumnTest, AllColumnsHelperNoTable) {
    const auto all_cols = all();

    EXPECT_TRUE(all_cols.table_name().empty());
}

TEST(ColumnTest, IsColumnConcept) {
    static_assert(IsColumnLike<Column>);
    static_assert(IsColumnLike<AllColumns>);

    static_assert(!IsColumnLike<int>);
    static_assert(!IsColumnLike<std::string>);
}

TEST(ColumnTest, IsColumnExactConcept) {
    static_assert(IsColumn<Column>);
    static_assert(!IsColumn<AllColumns>);
    static_assert(!IsColumn<int>);
}

TEST(ColumnTest, MultipleColumns) {
    Column id_col{"id", "products"};
    Column name_col{"name", "products"};
    Column price_col{"price", "products"};

    EXPECT_EQ(id_col.name(), "id");
    EXPECT_EQ(name_col.name(), "name");
    EXPECT_EQ(price_col.name(), "price");

    EXPECT_EQ(id_col.table_name(), "products");
    EXPECT_EQ(name_col.table_name(), "products");
    EXPECT_EQ(price_col.table_name(), "products");
}

TEST(ColumnTest, ColumnWithComplexAlias) {
    const Column column{"test_field", "very_long_table_name"};

    const Column aliased = column.as("short");
    EXPECT_EQ(aliased.table_name(), "very_long_table_name");
    EXPECT_EQ(aliased.alias(), "short");

    const Column re_aliased = column.as("even_shorter");
    EXPECT_EQ(re_aliased.alias(), "even_shorter");
    EXPECT_EQ(re_aliased.table_name(), "very_long_table_name");
}
