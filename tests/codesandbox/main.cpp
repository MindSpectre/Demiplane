#include <db_core_objects.hpp>
#include <iostream>

// Define compile-time schema (optional, for type-safe access)
struct UserSchema {
    static constexpr auto id   = demiplane::db::FieldDef<demiplane::gears::FixedString{"id"}, int>{};
    static constexpr auto name = demiplane::db::FieldDef<demiplane::gears::FixedString{"name"}, std::string>{};
    static constexpr auto age  = demiplane::db::FieldDef<demiplane::gears::FixedString{"age"}, int>{};

    using fields = demiplane::gears::type_list<decltype(id), decltype(name), decltype(age)>;
};

int main() {
    using namespace demiplane::db;

    // ═══════════════════════════════════════════════════════════════
    // APPROACH 1: Schema-Aware Constructor (Auto-Initialize)
    // ═══════════════════════════════════════════════════════════════

    // ✨ NEW: Pass schema to constructor - fields auto-initialized!
    Table users("users", UserSchema{});

    // Fields already exist - just configure them
    users.set_db_type(UserSchema::id, "SERIAL")
        .primary_key(UserSchema::id)
        .set_db_type(UserSchema::name, "VARCHAR(255)")
        .nullable(UserSchema::name, false)
        .set_db_type(UserSchema::age, "INTEGER")
        .indexed(UserSchema::age);

    // ═══════════════════════════════════════════════════════════════
    // APPROACH 2: Manual Field Addition (Traditional)
    // ═══════════════════════════════════════════════════════════════

    Table manual("manual");

    // Add fields manually at runtime
    manual.add_field<int>("id", "INTEGER")
        .add_field<std::string>("name", "VARCHAR(255)")
        .add_field<int>("age", "INTEGER");

    // Both APIs work - runtime and compile-time
    TableColumn<int> runtime_id       = manual.column<int>("id");         // Runtime
    TableColumn<int> compiletime_id   = users.column(UserSchema::id);    // Compile-time
    TableColumn<std::string> name_col = users.column(UserSchema::name);  // Compile-time

    // ❌ Uncomment to test compile-time error - type mismatch
    // TableColumn<std::string> wrong = users.column(UserSchema::id);
    // Error: cannot convert TableColumn<int> to TableColumn<std::string>

    std::cout << "✓ Unified Table API with schema constructor!\n";
    std::cout << "  - Table(name, Schema{}) - auto-initializes fields\n";
    std::cout << "  - Table(name) - manual field addition\n";
    std::cout << "  - column<T>(\"name\") - runtime type-safe\n";
    std::cout << "  - column(FieldDef) - compile-time type-safe\n";

    return 0;
}
