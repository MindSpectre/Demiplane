#include <string>
#include <typeindex>
#include <typeinfo>

#include <db_column.hpp>
#include <db_field_schema.hpp>

#include <gtest/gtest.h>

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
    const TableColumn<int> column(&test_schema, "test_table");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.table_name(), "test_table");
    EXPECT_FALSE(column.alias().has_value());
    EXPECT_EQ(column.name(), "test_field");
}

TEST_F(ColumnTest, TypedColumnWithAlias) {
    const TableColumn<int> column(&test_schema, "test_table", "t");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.table_name(), "test_table");
    EXPECT_EQ(column.alias().value(), "t");
    EXPECT_EQ(column.name(), "test_field");
}

TEST_F(ColumnTest, TypedColumnAliasing) {
    const TableColumn<int> original(&test_schema, "test_table");
    const TableColumn<int> aliased = original.as("t");

    EXPECT_FALSE(original.alias().has_value());
    EXPECT_EQ(aliased.alias().value(), "t");
    EXPECT_EQ(aliased.table_name(), "test_table");
    EXPECT_EQ(aliased.schema(), &test_schema);
}

TEST_F(ColumnTest, VoidColumnConstruction) {
    const TableColumn<void> column(&test_schema, "test_table");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.table_name(), "test_table");
    EXPECT_FALSE(column.alias().has_value());
    EXPECT_EQ(column.name(), "test_field");
}

TEST_F(ColumnTest, VoidColumnWithAlias) {
    const TableColumn<void> column(&test_schema, "test_table", "t");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.table_name(), "test_table");
    EXPECT_EQ(column.alias().value(), "t");
    EXPECT_EQ(column.name(), "test_field");
}


TEST_F(ColumnTest, AllColumnsWithTable) {
    const AllColumns all_cols("users");

    EXPECT_EQ(all_cols.table_name(), "users");
}

TEST_F(ColumnTest, ColumnCreationHelper) {
    const auto column = col<int>(&test_schema, "test_table");

    EXPECT_EQ(column.schema(), &test_schema);
    EXPECT_EQ(column.table_name(), "test_table");
    EXPECT_EQ(column.name(), "test_field");
}

TEST_F(ColumnTest, AllColumnsHelper) {
    const auto all_with_table = all("users");

    EXPECT_EQ(all_with_table.table_name(), "users");
}

TEST_F(ColumnTest, IsColumnConcept) {
    // Test that the concept works correctly
    static_assert(IsColumn<TableColumn<int>>);
    static_assert(IsColumn<TableColumn<std::string>>);
    static_assert(IsColumn<TableColumn<void>>);
    static_assert(IsColumn<AllColumns>);

    // Test that non-column types don't match
    static_assert(!IsColumn<int>);
    static_assert(!IsColumn<std::string>);
    static_assert(!IsColumn<FieldSchema>);
}

TEST_F(ColumnTest, ColumnValueType) {
    [[maybe_unused]] TableColumn<int> int_column(&test_schema, "test_table");
    [[maybe_unused]] TableColumn<std::string> string_column(&test_schema, "test_table");

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

    TableColumn<int> id_col(&test_schema, "products");
    TableColumn<std::string> name_col(&string_schema, "products");
    TableColumn<double> price_col(&double_schema, "products");

    EXPECT_EQ(id_col.name(), "test_field");
    EXPECT_EQ(name_col.name(), "name");
    EXPECT_EQ(price_col.name(), "price");

    EXPECT_EQ(id_col.table_name(), "products");
    EXPECT_EQ(name_col.table_name(), "products");
    EXPECT_EQ(price_col.table_name(), "products");
}

TEST_F(ColumnTest, ColumnWithComplexAlias) {
    const TableColumn<int> column(&test_schema, "very_long_table_name", "short");

    EXPECT_EQ(column.table_name(), "very_long_table_name");
    EXPECT_EQ(column.alias().value(), "short");

    const TableColumn<int> aliased = column.as("even_shorter");
    EXPECT_EQ(aliased.alias().value(), "even_shorter");
    EXPECT_EQ(aliased.table_name(), "very_long_table_name");
}
