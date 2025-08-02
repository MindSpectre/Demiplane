#include <gtest/gtest.h>
#include <typeindex>
#include <typeinfo>
#include <string>
#include <db_column.hpp>
#include <db_field_schema.hpp>

using namespace demiplane::db;

class ColumnTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_schema.name     = "test_field";
        test_schema.db_type  = "INTEGER";
        test_schema.cpp_type = std::type_index(typeid(int));
    }

    FieldSchema test_schema;
};

TEST_F(ColumnTest, TypedColumnConstruction) {
    Column<int> column(&test_schema, "test_table");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.view_table(), "test_table");
    EXPECT_FALSE(column.alias().has_value());
    EXPECT_EQ(column.name(), "test_field");
}

TEST_F(ColumnTest, TypedColumnWithAlias) {
    Column<int> column(&test_schema, "test_table", "t");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.view_table(), "test_table");
    EXPECT_EQ(column.alias().value(), "t");
    EXPECT_EQ(column.name(), "test_field");
}

TEST_F(ColumnTest, TypedColumnAliasing) {
    Column<int> original(&test_schema, "test_table");
    Column<int> aliased = original.as("t");

    EXPECT_FALSE(original.alias().has_value());
    EXPECT_EQ(aliased.alias().value(), "t");
    EXPECT_EQ(aliased.view_table(), "test_table");
    EXPECT_EQ(aliased.schema(), &test_schema);
}

TEST_F(ColumnTest, VoidColumnConstruction) {
    Column<void> column(&test_schema, "test_table");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.view_table(), "test_table");
    EXPECT_FALSE(column.alias().has_value());
    EXPECT_EQ(column.name(), "test_field");
}

TEST_F(ColumnTest, VoidColumnWithAlias) {
    Column<void> column(&test_schema, "test_table", "t");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.view_table(), "test_table");
    EXPECT_EQ(column.alias().value(), "t");
    EXPECT_EQ(column.name(), "test_field");
}


TEST_F(ColumnTest, AllColumnsWithTable) {
    AllColumns all_cols("users");

    EXPECT_EQ(all_cols.view_table(), "users");
}

TEST_F(ColumnTest, ColumnCreationHelper) {
    auto column = col<int>(&test_schema, "test_table");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.view_table(), "test_table");
    EXPECT_EQ(column.name(), "test_field");
}

TEST_F(ColumnTest, AllColumnsHelper) {
    auto all_with_table = all("users");

    EXPECT_EQ(all_with_table.view_table(), "users");
}

TEST_F(ColumnTest, IsColumnConcept) {
    // Test that the concept works correctly
    static_assert(IsColumn<Column<int>>);
    static_assert(IsColumn<Column<std::string>>);
    static_assert(IsColumn<Column<void>>);
    static_assert(IsColumn<AllColumns>);

    // Test that non-column types don't match
    static_assert(!IsColumn<int>);
    static_assert(!IsColumn<std::string>);
    static_assert(!IsColumn<FieldSchema>);
}

TEST_F(ColumnTest, ColumnValueType) {
    [[maybe_unused]] Column<int> int_column(&test_schema, "test_table");
    [[maybe_unused]] Column<std::string> string_column(&test_schema, "test_table");

    static_assert(std::is_same_v<typename decltype(int_column)::value_type, int>);
    static_assert(std::is_same_v<typename decltype(string_column)::value_type, std::string>);
}

TEST_F(ColumnTest, MultipleColumnTypes) {
    FieldSchema string_schema;
    string_schema.name     = "name";
    string_schema.db_type  = "VARCHAR(255)";
    string_schema.cpp_type = std::type_index(typeid(std::string));

    FieldSchema double_schema;
    double_schema.name     = "price";
    double_schema.db_type  = "DECIMAL(10,2)";
    double_schema.cpp_type = std::type_index(typeid(double));

    Column<int> id_col(&test_schema, "products");
    Column<std::string> name_col(&string_schema, "products");
    Column<double> price_col(&double_schema, "products");

    EXPECT_EQ(id_col.name(), "test_field");
    EXPECT_EQ(name_col.name(), "name");
    EXPECT_EQ(price_col.name(), "price");

    EXPECT_EQ(id_col.view_table(), "products");
    EXPECT_EQ(name_col.view_table(), "products");
    EXPECT_EQ(price_col.view_table(), "products");
}

TEST_F(ColumnTest, ColumnWithComplexAlias) {
    const Column<int> column(&test_schema, "very_long_table_name", "short");

    EXPECT_EQ(column.view_table(), "very_long_table_name");
    EXPECT_EQ(column.alias().value(), "short");

    const Column<int> aliased = column.as("even_shorter");
    EXPECT_EQ(aliased.alias().value(), "even_shorter");
    EXPECT_EQ(aliased.view_table(), "very_long_table_name");
}
