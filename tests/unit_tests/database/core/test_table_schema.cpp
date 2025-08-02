#include <db_column.hpp>
#include <db_table_schema.hpp>

#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <gtest/gtest.h>

using namespace demiplane::db;

class TableSchemaTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TableSchemaTest, TableSchemaConstruction) {
    const TableSchema schema("users");

    EXPECT_EQ(schema.table_name(), "users");
    EXPECT_EQ(schema.field_count(), 0);
    EXPECT_TRUE(schema.fields().empty());
    EXPECT_TRUE(schema.field_names().empty());
}

TEST_F(TableSchemaTest, AddFieldWithType) {
    TableSchema schema("users");

    schema.add_field<int>("id", "INTEGER");
    schema.add_field<std::string>("name", "VARCHAR(255)");
    schema.add_field<double>("balance", "DECIMAL(10,2)");

    EXPECT_EQ(schema.field_count(), 3);

    auto field_names = schema.field_names();
    EXPECT_EQ(field_names.size(), 3);
    EXPECT_TRUE(std::ranges::find(field_names, "id") != field_names.end());
    EXPECT_TRUE(std::ranges::find(field_names, "name") != field_names.end());
    EXPECT_TRUE(std::ranges::find(field_names, "balance") != field_names.end());
}

TEST_F(TableSchemaTest, AddFieldWithRuntimeType) {
    TableSchema schema("products");

    schema.add_field("id", "INTEGER", std::type_index(typeid(int)));
    schema.add_field("title", "TEXT", std::type_index(typeid(std::string)));

    EXPECT_EQ(schema.field_count(), 2);

    const auto* id_field    = schema.get_field_schema("id");
    const auto* title_field = schema.get_field_schema("title");

    ASSERT_NE(id_field, nullptr);
    ASSERT_NE(title_field, nullptr);

    EXPECT_EQ(id_field->name, "id");
    EXPECT_EQ(id_field->db_type, "INTEGER");
    EXPECT_EQ(id_field->cpp_type, std::type_index(typeid(int)));

    EXPECT_EQ(title_field->name, "title");
    EXPECT_EQ(title_field->db_type, "TEXT");
    EXPECT_EQ(title_field->cpp_type, std::type_index(typeid(std::string)));
}

TEST_F(TableSchemaTest, GetFieldSchema) {
    TableSchema schema("test_table");
    schema.add_field<int>("id", "INTEGER");
    schema.add_field<std::string>("email", "VARCHAR(255)");

    const auto* id_field          = schema.get_field_schema("id");
    const auto* email_field       = schema.get_field_schema("email");
    const auto* nonexistent_field = schema.get_field_schema("nonexistent");

    ASSERT_NE(id_field, nullptr);
    ASSERT_NE(email_field, nullptr);
    EXPECT_EQ(nonexistent_field, nullptr);

    EXPECT_EQ(id_field->name, "id");
    EXPECT_EQ(id_field->db_type, "INTEGER");
    EXPECT_EQ(email_field->name, "email");
    EXPECT_EQ(email_field->db_type, "VARCHAR(255)");
}

TEST_F(TableSchemaTest, GetFieldSchemaMutable) {
    TableSchema schema("test_table");
    schema.add_field<int>("id", "INTEGER");

    auto* id_field = schema.get_field_schema("id");
    ASSERT_NE(id_field, nullptr);

    // Modify field properties
    id_field->is_primary_key = true;
    id_field->is_nullable    = false;

    const auto* const_field = schema.get_field_schema("id");
    EXPECT_TRUE(const_field->is_primary_key);
    EXPECT_FALSE(const_field->is_nullable);
}

TEST_F(TableSchemaTest, PrimaryKey) {
    TableSchema schema("users");
    schema.add_field<int>("id", "INTEGER");
    schema.add_field<std::string>("email", "VARCHAR(255)");

    schema.primary_key("id");

    const auto* id_field    = schema.get_field_schema("id");
    const auto* email_field = schema.get_field_schema("email");

    ASSERT_NE(id_field, nullptr);
    ASSERT_NE(email_field, nullptr);

    EXPECT_TRUE(id_field->is_primary_key);
    EXPECT_FALSE(email_field->is_primary_key);
}

TEST_F(TableSchemaTest, Nullable) {
    TableSchema schema("users");
    schema.add_field<std::string>("name", "VARCHAR(255)");
    schema.add_field<std::string>("email", "VARCHAR(255)");

    schema.nullable("name", false);
    // email remains nullable by default

    const auto* name_field  = schema.get_field_schema("name");
    const auto* email_field = schema.get_field_schema("email");

    ASSERT_NE(name_field, nullptr);
    ASSERT_NE(email_field, nullptr);

    EXPECT_FALSE(name_field->is_nullable);
    EXPECT_TRUE(email_field->is_nullable);
}

TEST_F(TableSchemaTest, ForeignKey) {
    TableSchema schema("orders");
    schema.add_field<int>("id", "INTEGER");
    schema.add_field<int>("user_id", "INTEGER");

    schema.foreign_key("user_id", "users", "id");

    const auto* user_id_field = schema.get_field_schema("user_id");
    ASSERT_NE(user_id_field, nullptr);

    EXPECT_TRUE(user_id_field->is_foreign_key);
    EXPECT_EQ(user_id_field->foreign_table, "users");
    EXPECT_EQ(user_id_field->foreign_column, "id");
}

TEST_F(TableSchemaTest, Unique) {
    TableSchema schema("users");
    schema.add_field<std::string>("email", "VARCHAR(255)");
    schema.add_field<std::string>("username", "VARCHAR(50)");

    schema.unique("email");

    const auto* email_field    = schema.get_field_schema("email");
    const auto* username_field = schema.get_field_schema("username");

    ASSERT_NE(email_field, nullptr);
    ASSERT_NE(username_field, nullptr);

    EXPECT_TRUE(email_field->is_unique);
    EXPECT_FALSE(username_field->is_unique);
}

TEST_F(TableSchemaTest, Indexed) {
    TableSchema schema("users");
    schema.add_field<std::string>("last_name", "VARCHAR(100)");
    schema.add_field<std::string>("first_name", "VARCHAR(100)");

    schema.indexed("last_name");

    const auto* last_name_field  = schema.get_field_schema("last_name");
    const auto* first_name_field = schema.get_field_schema("first_name");

    ASSERT_NE(last_name_field, nullptr);
    ASSERT_NE(first_name_field, nullptr);

    EXPECT_TRUE(last_name_field->is_indexed);
    EXPECT_FALSE(first_name_field->is_indexed);
}

TEST_F(TableSchemaTest, ChainedBuilderPattern) {
    TableSchema schema("users");

    schema.add_field<int>("id", "INTEGER")
          .primary_key("id")
          .nullable("id", false)
          .add_field<std::string>("email", "VARCHAR(255)")
          .unique("email")
          .nullable("email", false)
          .indexed("email")
          .add_field<std::string>("name", "VARCHAR(100)")
          .nullable("name", true);

    EXPECT_EQ(schema.field_count(), 3);

    const auto* id_field    = schema.get_field_schema("id");
    const auto* email_field = schema.get_field_schema("email");
    const auto* name_field  = schema.get_field_schema("name");

    ASSERT_NE(id_field, nullptr);
    ASSERT_NE(email_field, nullptr);
    ASSERT_NE(name_field, nullptr);

    EXPECT_TRUE(id_field->is_primary_key);
    EXPECT_FALSE(id_field->is_nullable);

    EXPECT_TRUE(email_field->is_unique);
    EXPECT_FALSE(email_field->is_nullable);
    EXPECT_TRUE(email_field->is_indexed);

    EXPECT_TRUE(name_field->is_nullable);
    EXPECT_FALSE(name_field->is_unique);
}

TEST_F(TableSchemaTest, ComplexSchemaDefinition) {
    TableSchema schema("complex_table");

    schema.add_field<int>("id", "INTEGER")
          .primary_key("id")
          .nullable("id", false)
          .add_field<int>("parent_id", "INTEGER")
          .foreign_key("parent_id", "complex_table", "id")
          .add_field<std::string>("title", "VARCHAR(200)")
          .nullable("title", false)
          .indexed("title")
          .add_field<std::string>("slug", "VARCHAR(200)")
          .unique("slug")
          .nullable("slug", false)
          .add_field<std::string>("description", "TEXT");

    EXPECT_EQ(schema.field_count(), 5);

    auto field_names = schema.field_names();
    EXPECT_EQ(field_names.size(), 5);

    const auto* id_field     = schema.get_field_schema("id");
    const auto* parent_field = schema.get_field_schema("parent_id");
    const auto* title_field  = schema.get_field_schema("title");
    const auto* slug_field   = schema.get_field_schema("slug");
    const auto* desc_field   = schema.get_field_schema("description");

    ASSERT_NE(id_field, nullptr);
    ASSERT_NE(parent_field, nullptr);
    ASSERT_NE(title_field, nullptr);
    ASSERT_NE(slug_field, nullptr);
    ASSERT_NE(desc_field, nullptr);

    EXPECT_TRUE(id_field->is_primary_key);
    EXPECT_TRUE(parent_field->is_foreign_key);
    EXPECT_TRUE(title_field->is_indexed);
    EXPECT_TRUE(slug_field->is_unique);
    EXPECT_TRUE(desc_field->is_nullable);
}

TEST_F(TableSchemaTest, TypedColumnAccess) {
    TableSchema schema("users");
    schema.add_field<int>("id", "INTEGER");
    schema.add_field<std::string>("name", "VARCHAR(255)");

    const auto id_column   = schema.column<int>("id");
    const auto name_column = schema.column<std::string>("name");

    EXPECT_EQ(id_column.name(), "id");
    EXPECT_EQ(id_column.view_table(), "users");
    EXPECT_EQ(name_column.name(), "name");
    EXPECT_EQ(name_column.view_table(), "users");
}

TEST_F(TableSchemaTest, TableSchemaPtr) {
    const auto schema_ptr = std::make_shared<const TableSchema>("shared_table");

    EXPECT_EQ(schema_ptr->table_name(), "shared_table");
    EXPECT_EQ(schema_ptr->field_count(), 0);

    // Test concept
    static_assert(IsTableSchema<TableSchemaPtr>);
}
