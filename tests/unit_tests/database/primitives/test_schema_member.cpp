#include <string>
#include <type_traits>

#include <db_column.hpp>
#include <db_schema_macros.hpp>
#include <db_schema_member.hpp>
#include <db_table.hpp>
#include <gears_templates.hpp>
#include <gtest/gtest.h>

using namespace demiplane::db;
using demiplane::gears::FixedString;
using demiplane::gears::type_list;

// ═══════════════════════════════════════════════════════════════════════════
// Test Entity using DB_ENTITY macro
// ═══════════════════════════════════════════════════════════════════════════

struct User {
    int id;
    std::string name;
    int age;
    bool active;

    DB_ENTITY(User, "users", id, name, age, active);
};

// ═══════════════════════════════════════════════════════════════════════════
// SchemaMember Compile-Time Tests
// ═══════════════════════════════════════════════════════════════════════════

class SchemaMemberTest : public ::testing::Test {};

TEST_F(SchemaMemberTest, FieldNameIsCorrect) {
    EXPECT_EQ(User::Schema::id.name(), "id");
    EXPECT_EQ(User::Schema::name.name(), "name");
    EXPECT_EQ(User::Schema::age.name(), "age");
    EXPECT_EQ(User::Schema::active.name(), "active");
}

TEST_F(SchemaMemberTest, ValueTypeIsCorrect) {
    static_assert(std::is_same_v<decltype(User::Schema::id)::value_type, int>);
    static_assert(std::is_same_v<decltype(User::Schema::name)::value_type, std::string>);
    static_assert(std::is_same_v<decltype(User::Schema::age)::value_type, int>);
    static_assert(std::is_same_v<decltype(User::Schema::active)::value_type, bool>);
    SUCCEED();
}

TEST_F(SchemaMemberTest, TableNameIsCorrect) {
    EXPECT_EQ(User::Schema::table_name, "users");
}

// ═══════════════════════════════════════════════════════════════════════════
// SchemaMember satisfies IsFieldDef Concept
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SchemaMemberTest, SatisfiesIsFieldDefConcept) {
    static_assert(IsFieldDef<decltype(User::Schema::id)>);
    static_assert(IsFieldDef<decltype(User::Schema::name)>);
    static_assert(IsFieldDef<decltype(User::Schema::age)>);
    static_assert(IsFieldDef<decltype(User::Schema::active)>);
    SUCCEED();
}

TEST_F(SchemaMemberTest, SatisfiesHasSchemaInfoConcept) {
    static_assert(HasSchemaInfo<User::Schema>);
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════════
// Table Integration Tests - Using Table::make<Schema>()
// ═══════════════════════════════════════════════════════════════════════════

class SchemaTableIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        table = Table::make<User::Schema>();
    }

    std::shared_ptr<Table> table;
};

TEST_F(SchemaTableIntegrationTest, TableCreatedWithSchemaFields) {
    EXPECT_EQ(table->field_count(), 4);
    EXPECT_EQ(table->table_name(), "users");

    auto names = table->field_names();
    EXPECT_EQ(names.size(), 4);
    EXPECT_EQ(names[0], "id");
    EXPECT_EQ(names[1], "name");
    EXPECT_EQ(names[2], "age");
    EXPECT_EQ(names[3], "active");
}

TEST_F(SchemaTableIntegrationTest, ColumnAccessWithSchemaMember) {
    auto id_col     = table->column(User::Schema::id);
    auto name_col   = table->column(User::Schema::name);
    auto age_col    = table->column(User::Schema::age);
    auto active_col = table->column(User::Schema::active);

    EXPECT_EQ(id_col.name(), "id");
    EXPECT_EQ(name_col.name(), "name");
    EXPECT_EQ(age_col.name(), "age");
    EXPECT_EQ(active_col.name(), "active");
}

TEST_F(SchemaTableIntegrationTest, ColumnTypesAreCorrect) {
    [[maybe_unused]] auto id_col   = table->column(User::Schema::id);
    [[maybe_unused]] auto name_col = table->column(User::Schema::name);

    static_assert(std::is_same_v<typename decltype(id_col)::value_type, int>);
    static_assert(std::is_same_v<typename decltype(name_col)::value_type, std::string>);
    SUCCEED();
}

TEST_F(SchemaTableIntegrationTest, ColumnTableNameIsCorrect) {
    auto id_col = table->column(User::Schema::id);
    EXPECT_EQ(id_col.table_name(), "users");
}

// ═══════════════════════════════════════════════════════════════════════════
// Type List Integration Tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(TypeListTest, FieldsTypeListIsCorrect) {
    using Fields = User::Schema::fields;

    // Verify the type list contains the expected number of fields
    constexpr auto count = []<typename... Ts>(type_list<Ts...>) { return sizeof...(Ts); }(Fields{});

    static_assert(count == 4);
    SUCCEED();
}

TEST(TypeListTest, IterateOverFields) {
    using Fields = User::Schema::fields;

    std::vector<std::string> names;

    []<typename... Ts>(type_list<Ts...>, std::vector<std::string>& out) {
        (out.emplace_back(Ts::name()), ...);
    }(Fields{}, names);

    ASSERT_EQ(names.size(), 4);
    EXPECT_EQ(names[0], "id");
    EXPECT_EQ(names[1], "name");
    EXPECT_EQ(names[2], "age");
    EXPECT_EQ(names[3], "active");
}

// ═══════════════════════════════════════════════════════════════════════════
// DB_ENTITY Macro Tests - Various Entity Types
// ═══════════════════════════════════════════════════════════════════════════

struct Product {
    int id;
    std::string name;
    double price;
    int quantity;
    bool available;

    DB_ENTITY(Product, "products", id, name, price, quantity, available);
};

class EntityMacroTest : public ::testing::Test {};

TEST_F(EntityMacroTest, FieldNamesAreCorrect) {
    EXPECT_EQ(Product::Schema::id.name(), "id");
    EXPECT_EQ(Product::Schema::name.name(), "name");
    EXPECT_EQ(Product::Schema::price.name(), "price");
    EXPECT_EQ(Product::Schema::quantity.name(), "quantity");
    EXPECT_EQ(Product::Schema::available.name(), "available");
}

TEST_F(EntityMacroTest, FieldTypesAreCorrect) {
    static_assert(std::is_same_v<decltype(Product::Schema::id)::value_type, int>);
    static_assert(std::is_same_v<decltype(Product::Schema::name)::value_type, std::string>);
    static_assert(std::is_same_v<decltype(Product::Schema::price)::value_type, double>);
    static_assert(std::is_same_v<decltype(Product::Schema::quantity)::value_type, int>);
    static_assert(std::is_same_v<decltype(Product::Schema::available)::value_type, bool>);
    SUCCEED();
}

TEST_F(EntityMacroTest, TableNameIsCorrect) {
    EXPECT_EQ(Product::Schema::table_name, "products");
}

TEST_F(EntityMacroTest, FieldsTypeListIsCorrect) {
    using Fields = Product::Schema::fields;

    constexpr auto count = []<typename... Ts>(type_list<Ts...>) { return sizeof...(Ts); }(Fields{});

    static_assert(count == 5);
    SUCCEED();
}

TEST_F(EntityMacroTest, TableIntegrationWorks) {
    auto table = Table::make<Product::Schema>();

    EXPECT_EQ(table->field_count(), 5);
    EXPECT_EQ(table->table_name(), "products");

    auto id_col    = table->column(Product::Schema::id);
    auto name_col  = table->column(Product::Schema::name);
    auto price_col = table->column(Product::Schema::price);

    EXPECT_EQ(id_col.name(), "id");
    EXPECT_EQ(name_col.name(), "name");
    EXPECT_EQ(price_col.name(), "price");

    static_assert(std::is_same_v<typename decltype(id_col)::value_type, int>);
    static_assert(std::is_same_v<typename decltype(price_col)::value_type, double>);
}

// ═══════════════════════════════════════════════════════════════════════════
// Edge Cases - Minimal and Large Entities
// ═══════════════════════════════════════════════════════════════════════════

// Single field entity
struct MinimalEntity {
    int id;

    DB_ENTITY(MinimalEntity, "minimal", id);
};

TEST(EntityEdgeCaseTest, SingleFieldWorks) {
    EXPECT_EQ(MinimalEntity::Schema::id.name(), "id");
    EXPECT_EQ(MinimalEntity::Schema::table_name, "minimal");

    static_assert(std::is_same_v<decltype(MinimalEntity::Schema::id)::value_type, int>);

    using Fields         = MinimalEntity::Schema::fields;
    constexpr auto count = []<typename... Ts>(type_list<Ts...>) { return sizeof...(Ts); }(Fields{});
    static_assert(count == 1);

    auto table = Table::make<MinimalEntity::Schema>();
    EXPECT_EQ(table->field_count(), 1);
}

// Ten field entity
struct LargeEntity {
    int f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;

    DB_ENTITY(LargeEntity, "large", f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);
};

TEST(EntityEdgeCaseTest, TenFieldsWork) {
    using Fields         = LargeEntity::Schema::fields;
    constexpr auto count = []<typename... Ts>(type_list<Ts...>) { return sizeof...(Ts); }(Fields{});
    static_assert(count == 10);

    auto table = Table::make<LargeEntity::Schema>();
    EXPECT_EQ(table->field_count(), 10);

    auto col1  = table->column(LargeEntity::Schema::f1);
    auto col10 = table->column(LargeEntity::Schema::f10);
    EXPECT_EQ(col1.name(), "f1");
    EXPECT_EQ(col10.name(), "f10");
}

// Entity with various types
struct Order {
    int id;
    int user_id;
    double amount;
    bool completed;

    DB_ENTITY(Order, "orders", id, user_id, amount, completed);
};

TEST_F(EntityMacroTest, OrderEntityWorks) {
    // Verify table name
    EXPECT_EQ(Order::Schema::table_name, "orders");

    // Verify field names
    EXPECT_EQ(Order::Schema::id.name(), "id");
    EXPECT_EQ(Order::Schema::user_id.name(), "user_id");
    EXPECT_EQ(Order::Schema::amount.name(), "amount");
    EXPECT_EQ(Order::Schema::completed.name(), "completed");

    // Verify types
    static_assert(std::is_same_v<decltype(Order::Schema::id)::value_type, int>);
    static_assert(std::is_same_v<decltype(Order::Schema::amount)::value_type, double>);
    static_assert(std::is_same_v<decltype(Order::Schema::completed)::value_type, bool>);

    // Verify type_list
    using Fields         = Order::Schema::fields;
    constexpr auto count = []<typename... Ts>(type_list<Ts...>) { return sizeof...(Ts); }(Fields{});
    static_assert(count == 4);

    // Verify table integration
    auto table = Table::make<Order::Schema>();
    EXPECT_EQ(table->field_count(), 4);

    auto col = table->column(Order::Schema::amount);
    EXPECT_EQ(col.name(), "amount");
    static_assert(std::is_same_v<typename decltype(col)::value_type, double>);
}

// Customer entity (replaces old DB_ENTITY test)
struct Customer {
    int id;
    std::string name;
    std::string email;
    bool active;

    DB_ENTITY(Customer, "customers", id, name, email, active);
};

TEST_F(EntityMacroTest, CustomerEntityWorks) {
    // Verify table name
    EXPECT_EQ(Customer::Schema::table_name, "customers");

    // Verify field names
    EXPECT_EQ(Customer::Schema::id.name(), "id");
    EXPECT_EQ(Customer::Schema::name.name(), "name");
    EXPECT_EQ(Customer::Schema::email.name(), "email");
    EXPECT_EQ(Customer::Schema::active.name(), "active");

    // Verify types
    static_assert(std::is_same_v<decltype(Customer::Schema::id)::value_type, int>);
    static_assert(std::is_same_v<decltype(Customer::Schema::name)::value_type, std::string>);
    static_assert(std::is_same_v<decltype(Customer::Schema::email)::value_type, std::string>);
    static_assert(std::is_same_v<decltype(Customer::Schema::active)::value_type, bool>);

    // Verify type_list count
    using Fields         = Customer::Schema::fields;
    constexpr auto count = []<typename... Ts>(type_list<Ts...>) { return sizeof...(Ts); }(Fields{});
    static_assert(count == 4);

    // Verify table integration with Table::make<Schema>()
    auto table = Table::make<Customer::Schema>();
    EXPECT_EQ(table->field_count(), 4);
    EXPECT_EQ(table->table_name(), "customers");

    auto col = table->column(Customer::Schema::email);
    EXPECT_EQ(col.name(), "email");
    static_assert(std::is_same_v<typename decltype(col)::value_type, std::string>);
}
