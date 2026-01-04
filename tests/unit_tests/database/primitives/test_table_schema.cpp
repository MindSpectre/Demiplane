#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <type_traits>

#include <db_core_objects.hpp>

#include <gtest/gtest.h>

using namespace demiplane::db;
using namespace demiplane::gears;

// Compile-time field definitions (optional, for type-safe access)
struct UsersFields {
    static constexpr auto id     = FieldDef<FixedString{"id"}, int>{};
    static constexpr auto name   = FieldDef<FixedString{"name"}, std::string>{};
    static constexpr auto age    = FieldDef<FixedString{"age"}, int>{};
    static constexpr auto active = FieldDef<FixedString{"active"}, bool>{};
};

struct ProductsFields {
    static constexpr auto id    = FieldDef<FixedString{"id"}, int>{};
    static constexpr auto title = FieldDef<FixedString{"title"}, std::string>{};
    static constexpr auto price = FieldDef<FixedString{"price"}, double>{};
};

class TableTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

TEST_F(TableTest, TableConstruction) {
    const Table schema("users");

    EXPECT_EQ(schema.table_name(), "users");
    EXPECT_EQ(schema.field_count(), 0);
    EXPECT_TRUE(schema.fields().empty());
    EXPECT_TRUE(schema.field_names().empty());
}

TEST_F(TableTest, AddFieldWithType) {
    Table schema("users");

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

TEST_F(TableTest, AddFieldWithRuntimeType) {
    Table schema("products");

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

TEST_F(TableTest, GetFieldSchema) {
    Table schema("test_table");
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

TEST_F(TableTest, GetFieldSchemaMutable) {
    Table schema("test_table");
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

TEST_F(TableTest, PrimaryKey) {
    Table schema("users");
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

TEST_F(TableTest, Nullable) {
    Table schema("users");
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

TEST_F(TableTest, ForeignKey) {
    Table schema("orders");
    schema.add_field<int>("id", "INTEGER");
    schema.add_field<int>("user_id", "INTEGER");

    schema.foreign_key("user_id", "users", "id");

    const auto* user_id_field = schema.get_field_schema("user_id");
    ASSERT_NE(user_id_field, nullptr);

    EXPECT_TRUE(user_id_field->is_foreign_key);
    EXPECT_EQ(user_id_field->foreign_table, "users");
    EXPECT_EQ(user_id_field->foreign_column, "id");
}

TEST_F(TableTest, Unique) {
    Table schema("users");
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

TEST_F(TableTest, Indexed) {
    Table schema("users");
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

TEST_F(TableTest, ChainedBuilderPattern) {
    Table schema("users");

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

TEST_F(TableTest, ComplexSchemaDefinition) {
    Table schema("complex_table");

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

TEST_F(TableTest, TypedColumnAccess) {
    Table schema("users");
    schema.add_field<int>("id", "INTEGER");
    schema.add_field<std::string>("name", "VARCHAR(255)");

    const auto id_column   = schema.column<int>("id");
    const auto name_column = schema.column<std::string>("name");

    EXPECT_EQ(id_column.name(), "id");
    EXPECT_EQ(id_column.table_name(), "users");
    EXPECT_EQ(name_column.name(), "name");
    EXPECT_EQ(name_column.table_name(), "users");
}

TEST_F(TableTest, TablePtr) {
    const auto schema_ptr = std::make_shared<const Table>("shared_table");

    EXPECT_EQ(schema_ptr->table_name(), "shared_table");
    EXPECT_EQ(schema_ptr->field_count(), 0);

    // Test concept
    static_assert(IsTable<TablePtr>);
}

// ═══════════════════════════════════════════════════════════════
// UNIFIED API TESTS (compile-time + runtime)
// ═══════════════════════════════════════════════════════════════

TEST_F(TableTest, CompileTimeColumnAccess) {
    Table users("users");
    users.add_field<int>("id", "INTEGER")
        .add_field<std::string>("name", "VARCHAR(255)")
        .add_field<int>("age", "INTEGER");

    // ✅ Compile-time API - type inferred from FieldDef
    TableColumn<int> id_col           = users.column(UsersFields::id);
    TableColumn<std::string> name_col = users.column(UsersFields::name);
    TableColumn<int> age_col          = users.column(UsersFields::age);

    EXPECT_EQ(id_col.name(), "id");
    EXPECT_EQ(name_col.name(), "name");
    EXPECT_EQ(age_col.name(), "age");
}

TEST_F(TableTest, CompileTimeBuilders) {
    Table users("users");
    users.add_field<int>("id", "INTEGER")
        .add_field<std::string>("name", "VARCHAR(255)")
        .add_field<int>("age", "INTEGER");

    // ✅ Compile-time builders - use FieldDef overloads
    users.set_db_type(UsersFields::id, "SERIAL")
        .primary_key(UsersFields::id)
        .set_db_type(UsersFields::name, "VARCHAR(255)")
        .nullable(UsersFields::name, false)
        .unique(UsersFields::name)
        .set_db_type(UsersFields::age, "INTEGER")
        .indexed(UsersFields::age);

    const auto* id_schema   = users.get_field_schema("id");
    const auto* name_schema = users.get_field_schema("name");
    const auto* age_schema  = users.get_field_schema("age");

    EXPECT_EQ(id_schema->db_type, "SERIAL");
    EXPECT_TRUE(id_schema->is_primary_key);
    EXPECT_EQ(name_schema->db_type, "VARCHAR(255)");
    EXPECT_FALSE(name_schema->is_nullable);
    EXPECT_TRUE(name_schema->is_unique);
    EXPECT_EQ(age_schema->db_type, "INTEGER");
    EXPECT_TRUE(age_schema->is_indexed);
}

TEST_F(TableTest, CompileTimeDatabaseAttributes) {
    Table users("users");
    users.add_field<int>("id", "INTEGER");

    users.add_db_attribute(UsersFields::id, "COLLATE", "en_US")
        .add_db_attribute(UsersFields::id, "GENERATED", "ALWAYS");

    const auto* id_schema = users.get_field_schema("id");
    EXPECT_EQ(id_schema->db_attributes.at("COLLATE"), "en_US");
    EXPECT_EQ(id_schema->db_attributes.at("GENERATED"), "ALWAYS");
}

TEST_F(TableTest, MixedRuntimeAndCompileTime) {
    Table users("users");

    // Add fields at runtime
    users.add_field<int>("id", "INTEGER").add_field<std::string>("name", "VARCHAR(255)");

    // Use runtime builders
    users.primary_key("id");

    // Use compile-time builders on same table!
    users.set_db_type(UsersFields::name, "TEXT").nullable(UsersFields::name, false);

    // Access with runtime API
    TableColumn<int> runtime_id = users.column<int>("id");

    // Access with compile-time API
    TableColumn<std::string> compiletime_name = users.column(UsersFields::name);

    EXPECT_EQ(runtime_id.name(), "id");
    EXPECT_EQ(compiletime_name.name(), "name");

    const auto* id_schema   = users.get_field_schema("id");
    const auto* name_schema = users.get_field_schema("name");

    EXPECT_TRUE(id_schema->is_primary_key);
    EXPECT_FALSE(name_schema->is_nullable);
}

TEST_F(TableTest, MultipleTablesIndependent) {
    Table users("users");
    Table products("products");

    users.add_field<int>("id", "INTEGER").set_db_type(UsersFields::id, "INTEGER");
    products.add_field<int>("id", "INTEGER").set_db_type(ProductsFields::id, "BIGINT");

    const auto* user_id_schema    = users.get_field_schema("id");
    const auto* product_id_schema = products.get_field_schema("id");

    EXPECT_EQ(user_id_schema->db_type, "INTEGER");
    EXPECT_EQ(product_id_schema->db_type, "BIGINT");
}

// ═══════════════════════════════════════════════════════════════
// SCHEMA-AWARE CONSTRUCTOR TESTS
// ═══════════════════════════════════════════════════════════════

// Schema definition for testing auto-initialization
struct TestSchemaFields {
    static constexpr auto id   = FieldDef<FixedString{"id"}, int>{};
    static constexpr auto name = FieldDef<FixedString{"name"}, std::string>{};
    static constexpr auto age  = FieldDef<FixedString{"age"}, int>{};

    using fields = type_list<decltype(id), decltype(name), decltype(age)>;
};

TEST_F(TableTest, SchemaAwareConstructor) {
    // ✨ NEW: Schema-aware constructor - auto-initializes fields!
    Table users("users", TestSchemaFields{});

    // Fields should already exist
    EXPECT_EQ(users.field_count(), 3);

    const auto* id_schema   = users.get_field_schema("id");
    const auto* name_schema = users.get_field_schema("name");
    const auto* age_schema  = users.get_field_schema("age");

    ASSERT_NE(id_schema, nullptr);
    ASSERT_NE(name_schema, nullptr);
    ASSERT_NE(age_schema, nullptr);

    EXPECT_EQ(id_schema->name, "id");
    EXPECT_EQ(name_schema->name, "name");
    EXPECT_EQ(age_schema->name, "age");

    // Type checking
    EXPECT_EQ(id_schema->cpp_type, std::type_index(typeid(int)));
    EXPECT_EQ(name_schema->cpp_type, std::type_index(typeid(std::string)));
    EXPECT_EQ(age_schema->cpp_type, std::type_index(typeid(int)));
}

TEST_F(TableTest, SchemaAwareConstructorWithConfiguration) {
    Table users("users", TestSchemaFields{});

    // Configure fields using compile-time API
    users.set_db_type(TestSchemaFields::id, "SERIAL")
        .primary_key(TestSchemaFields::id)
        .set_db_type(TestSchemaFields::name, "VARCHAR(255)")
        .nullable(TestSchemaFields::name, false)
        .set_db_type(TestSchemaFields::age, "INTEGER")
        .indexed(TestSchemaFields::age);

    const auto* id_schema   = users.get_field_schema("id");
    const auto* name_schema = users.get_field_schema("name");
    const auto* age_schema  = users.get_field_schema("age");

    EXPECT_EQ(id_schema->db_type, "SERIAL");
    EXPECT_TRUE(id_schema->is_primary_key);
    EXPECT_EQ(name_schema->db_type, "VARCHAR(255)");
    EXPECT_FALSE(name_schema->is_nullable);
    EXPECT_EQ(age_schema->db_type, "INTEGER");
    EXPECT_TRUE(age_schema->is_indexed);
}

TEST_F(TableTest, SchemaAwareConstructorColumnAccess) {
    Table users("users", TestSchemaFields{});

    // Compile-time type-safe column access
    TableColumn<int> id_col           = users.column(TestSchemaFields::id);
    TableColumn<std::string> name_col = users.column(TestSchemaFields::name);
    TableColumn<int> age_col          = users.column(TestSchemaFields::age);

    EXPECT_EQ(id_col.name(), "id");
    EXPECT_EQ(id_col.table_name(), "users");
    EXPECT_EQ(name_col.name(), "name");
    EXPECT_EQ(name_col.table_name(), "users");
    EXPECT_EQ(age_col.name(), "age");
    EXPECT_EQ(age_col.table_name(), "users");
}

// Note: The following would fail to compile (compile-time type safety!)
// TEST_F(TableTest, TypeMismatchError) {
//     Table users("users", TestSchemaFields{});
//     // ❌ This should NOT compile - type mismatch
//     TableColumn<std::string> wrong = users.column(TestSchemaFields::id);
//     // Error: cannot convert TableColumn<int> to TableColumn<std::string>
// }
