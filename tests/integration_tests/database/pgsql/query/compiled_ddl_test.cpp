// Compiled DDL Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

// Test fixture for compiled DDL queries
class CompiledDdlTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        // Clean up any leftover test tables
        CleanupTestTables();
    }

    void TearDown() override {
        if (connection()) {
            CleanupTestTables();
        }
        PgsqlTestFixture::TearDown();
    }

    void CleanupTestTables() const {
        (void)executor().execute("DROP TABLE IF EXISTS ddl_orders_test CASCADE");
        (void)executor().execute("DROP TABLE IF EXISTS ddl_constraints_test CASCADE");
        (void)executor().execute("DROP TABLE IF EXISTS ddl_settings_test CASCADE");
        (void)executor().execute("DROP TABLE IF EXISTS ddl_temp_table CASCADE");
        (void)executor().execute("DROP TABLE IF EXISTS ddl_test_table CASCADE");
    }

    [[nodiscard]] bool TableExists(const std::string& table_name) const {
        auto result = executor().execute("SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = '" +
                                         table_name + "')");
        if (result.is_success() && result.value().rows() > 0) {
            return result.value().get<bool>(0, 0);
        }
        return false;
    }

    [[nodiscard]] int CountTableColumns(const std::string& table_name) const {
        auto result = executor().execute("SELECT COUNT(*) FROM information_schema.columns WHERE table_name = '" +
                                         table_name + "'");
        if (result.is_success() && result.value().rows() > 0) {
            return result.value().get<int>(0, 0);
        }
        return 0;
    }

    [[nodiscard]] bool ColumnExists(const std::string& table_name, const std::string& column_name) const {
        auto result =
            executor().execute("SELECT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_name = '" +
                               table_name + "' AND column_name = '" + column_name + "')");
        if (result.is_success() && result.value().rows() > 0) {
            return result.value().get<bool>(0, 0);
        }
        return false;
    }

    [[nodiscard]] bool HasPrimaryKey(const std::string& table_name) const {
        auto result = executor().execute("SELECT EXISTS (SELECT 1 FROM information_schema.table_constraints "
                                         "WHERE table_name = '" +
                                         table_name + "' AND constraint_type = 'PRIMARY KEY')");
        if (result.is_success() && result.value().rows() > 0) {
            return result.value().get<bool>(0, 0);
        }
        return false;
    }

    [[nodiscard]] bool HasUniqueConstraint(const std::string& table_name, const std::string& column_name) const {
        auto result = executor().execute(
            "SELECT EXISTS (SELECT 1 FROM information_schema.constraint_column_usage ccu "
            "JOIN information_schema.table_constraints tc ON ccu.constraint_name = tc.constraint_name "
            "WHERE tc.table_name = '" +
            table_name + "' AND ccu.column_name = '" + column_name + "' AND tc.constraint_type = 'UNIQUE')");
        if (result.is_success() && result.value().rows() > 0) {
            return result.value().get<bool>(0, 0);
        }
        return false;
    }

    [[nodiscard]] bool HasForeignKey(const std::string& table_name) const {
        auto result = executor().execute("SELECT EXISTS (SELECT 1 FROM information_schema.table_constraints "
                                         "WHERE table_name = '" +
                                         table_name + "' AND constraint_type = 'FOREIGN KEY')");
        if (result.is_success() && result.value().rows() > 0) {
            return result.value().get<bool>(0, 0);
        }
        return false;
    }

    [[nodiscard]] bool ColumnIsNotNull(const std::string& table_name, const std::string& column_name) const {
        auto result = executor().execute("SELECT is_nullable FROM information_schema.columns WHERE table_name = '" +
                                         table_name + "' AND column_name = '" + column_name + "'");
        if (result.is_success() && result.value().rows() > 0) {
            auto nullable = result.value().get<std::string>(0, 0);
            return nullable == "NO";
        }
        return false;
    }

    [[nodiscard]] std::string GetColumnDefault(const std::string& table_name, const std::string& column_name) const {
        auto result = executor().execute("SELECT column_default FROM information_schema.columns WHERE table_name = '" +
                                         table_name + "' AND column_name = '" + column_name + "'");
        if (result.is_success() && result.value().rows() > 0) {
            auto opt = result.value().get_opt<std::string>(0, 0);
            return opt.value_or("");
        }
        return "";
    }
};

// ============== CREATE TABLE Execution Tests ==============

TEST_F(CompiledDdlTest, CreateTableBasicExecutes) {
    // First ensure users table doesn't exist
    (void)executor().execute("DROP TABLE IF EXISTS users CASCADE");

    auto query  = library().produce<ddl::CreateTableBasic>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "CREATE TABLE failed: " << result.error<ErrorContext>();
    EXPECT_TRUE(TableExists("users"));
    EXPECT_TRUE(ColumnExists("users", "id"));
    EXPECT_TRUE(ColumnExists("users", "name"));
    EXPECT_TRUE(ColumnExists("users", "age"));
    EXPECT_TRUE(ColumnExists("users", "active"));

    // Cleanup
    (void)executor().execute("DROP TABLE IF EXISTS users CASCADE");
}

TEST_F(CompiledDdlTest, CreateTableIfNotExistsDoesNotFail) {
    // First ensure users table doesn't exist
    (void)executor().execute("DROP TABLE IF EXISTS users CASCADE");

    // Create once
    auto query1  = library().produce<ddl::CreateTableIfNotExists>();
    auto result1 = executor().execute(query1);
    ASSERT_TRUE(result1.is_success()) << "First CREATE failed: " << result1.error<ErrorContext>();

    // Create again - should not fail due to IF NOT EXISTS
    auto query2  = library().produce<ddl::CreateTableIfNotExists>();
    auto result2 = executor().execute(query2);
    ASSERT_TRUE(result2.is_success()) << "Second CREATE should not fail with IF NOT EXISTS";

    EXPECT_TRUE(TableExists("users"));

    // Cleanup
    (void)executor().execute("DROP TABLE IF EXISTS users CASCADE");
}

TEST_F(CompiledDdlTest, CreateTableWithConstraintsExecutes) {
    auto query  = library().produce<ddl::CreateTableWithConstraints>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "CREATE TABLE failed: " << result.error<ErrorContext>();

    EXPECT_TRUE(TableExists("ddl_constraints_test"));
    EXPECT_TRUE(HasPrimaryKey("ddl_constraints_test"));
    EXPECT_TRUE(ColumnIsNotNull("ddl_constraints_test", "email"));
    EXPECT_TRUE(ColumnIsNotNull("ddl_constraints_test", "name"));
    EXPECT_TRUE(HasUniqueConstraint("ddl_constraints_test", "email"));

    // Verify we can insert data
    auto insert_result = executor().execute(
        "INSERT INTO ddl_constraints_test (email, name, status) VALUES ('test@example.com', 'Test User', 1)");
    EXPECT_TRUE(insert_result.is_success()) << "INSERT failed: " << insert_result.error<ErrorContext>();

    // Verify UNIQUE constraint works - duplicate email should fail
    auto dup_result = executor().execute(
        "INSERT INTO ddl_constraints_test (email, name, status) VALUES ('test@example.com', 'Another User', 2)");
    EXPECT_FALSE(dup_result.is_success()) << "Duplicate email should violate UNIQUE constraint";
}

TEST_F(CompiledDdlTest, CreateTableWithForeignKeyExecutes) {
    // Create parent table first (users)
    CreateUsersTable();
    InsertTestUsers();

    auto query  = library().produce<ddl::CreateTableWithForeignKey>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "CREATE TABLE failed: " << result.error<ErrorContext>();

    EXPECT_TRUE(TableExists("ddl_orders_test"));
    EXPECT_TRUE(HasForeignKey("ddl_orders_test"));

    // Verify we can insert data with valid FK
    auto insert_result = executor().execute("INSERT INTO ddl_orders_test (user_id, amount) VALUES (1, 99.99)");
    EXPECT_TRUE(insert_result.is_success()) << "INSERT with valid FK failed";

    // Verify FK constraint works - invalid user_id should fail
    auto invalid_result = executor().execute("INSERT INTO ddl_orders_test (user_id, amount) VALUES (9999, 50.00)");
    EXPECT_FALSE(invalid_result.is_success()) << "Invalid FK should be rejected";

    // Cleanup
    DropUsersTable();
}

TEST_F(CompiledDdlTest, CreateTableWithDefaultExecutes) {
    auto query  = library().produce<ddl::CreateTableWithDefault>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "CREATE TABLE failed: " << result.error<ErrorContext>();

    EXPECT_TRUE(TableExists("ddl_settings_test"));

    // Verify default values are set
    auto enabled_default = GetColumnDefault("ddl_settings_test", "enabled");
    EXPECT_FALSE(enabled_default.empty()) << "enabled should have a default";
    EXPECT_TRUE(enabled_default.find("true") != std::string::npos) << "enabled default should be true";

    auto priority_default = GetColumnDefault("ddl_settings_test", "priority");
    EXPECT_FALSE(priority_default.empty()) << "priority should have a default";

    // Insert without specifying defaults and verify they are applied
    auto insert_result = executor().execute("INSERT INTO ddl_settings_test DEFAULT VALUES");
    EXPECT_TRUE(insert_result.is_success()) << "INSERT with defaults failed";

    auto select_result = executor().execute("SELECT enabled, priority FROM ddl_settings_test WHERE id = 1");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
    EXPECT_EQ(select_result.value().get<bool>(0, 0), true);
    EXPECT_EQ(select_result.value().get<int>(0, 1), 0);
}

// ============== DROP TABLE Execution Tests ==============

TEST_F(CompiledDdlTest, DropTableBasicExecutes) {
    // Create a table to drop
    CreateUsersTable();
    ASSERT_TRUE(TableExists("users"));

    auto query  = library().produce<ddl::DropTableBasic>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "DROP TABLE failed: " << result.error<ErrorContext>();
    EXPECT_FALSE(TableExists("users"));
}

TEST_F(CompiledDdlTest, DropTableIfExistsDoesNotFail) {
    // Ensure table doesn't exist
    (void)executor().execute("DROP TABLE IF EXISTS users CASCADE");
    ASSERT_FALSE(TableExists("users"));

    // DROP IF EXISTS on non-existent table should succeed
    auto query  = library().produce<ddl::DropTableIfExists>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "DROP TABLE IF EXISTS should not fail on non-existent table";
}

TEST_F(CompiledDdlTest, DropTableCascadeExecutes) {
    // Create parent and child tables
    CreateUsersTable();
    auto create_child =
        executor().execute("CREATE TABLE child_table (id SERIAL PRIMARY KEY, user_id INTEGER REFERENCES users(id))");
    ASSERT_TRUE(create_child.is_success());

    // Insert data
    InsertTestUsers();
    auto insert_child = executor().execute("INSERT INTO child_table (user_id) VALUES (1)");
    ASSERT_TRUE(insert_child.is_success());

    // DROP CASCADE should work even with dependent table
    auto query  = library().produce<ddl::DropTableCascade>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "DROP TABLE CASCADE failed: " << result.error<ErrorContext>();
    EXPECT_FALSE(TableExists("users"));

    // Cleanup child table
    (void)executor().execute("DROP TABLE IF EXISTS child_table CASCADE");
}

TEST_F(CompiledDdlTest, DropTableIfExistsCascadeExecutes) {
    // Create table with dependent
    CreateUsersTable();
    auto create_dep = executor().execute(
        "CREATE TABLE dependent_table (id SERIAL PRIMARY KEY, user_id INTEGER REFERENCES users(id))");
    ASSERT_TRUE(create_dep.is_success());

    auto query  = library().produce<ddl::DropTableIfExistsCascade>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "DROP TABLE IF EXISTS CASCADE failed";
    EXPECT_FALSE(TableExists("users"));

    // Cleanup
    (void)executor().execute("DROP TABLE IF EXISTS dependent_table CASCADE");
}

TEST_F(CompiledDdlTest, DropTableByNameExecutes) {
    // Create the temp table
    auto create_result = executor().execute("CREATE TABLE ddl_temp_table (id SERIAL PRIMARY KEY, data TEXT)");
    ASSERT_TRUE(create_result.is_success());
    ASSERT_TRUE(TableExists("ddl_temp_table"));

    auto query  = library().produce<ddl::DropTableByName>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "DROP TABLE by name failed: " << result.error<ErrorContext>();
    EXPECT_FALSE(TableExists("ddl_temp_table"));
}

// ============== Full Lifecycle Tests ==============

TEST_F(CompiledDdlTest, CreateInsertSelectDropLifecycle) {
    // Create table using DDL expression
    auto table = std::make_shared<Table>("ddl_test_table");
    table->add_field<int>("id", "SERIAL").primary_key("id");
    table->add_field<std::string>("name", "VARCHAR(100)").nullable("name", false);
    table->add_field<int>("value", "INTEGER");

    auto create_expr   = create_table(table, true);
    auto create_query  = library().compiler().compile(create_expr);
    auto create_result = executor().execute(create_query);
    ASSERT_TRUE(create_result.is_success()) << "CREATE failed: " << create_result.error<ErrorContext>();
    EXPECT_TRUE(TableExists("ddl_test_table"));

    // Insert data
    auto insert_result = executor().execute("INSERT INTO ddl_test_table (name, value) VALUES ('Test', 42)");
    ASSERT_TRUE(insert_result.is_success()) << "INSERT failed";

    // Select and verify
    auto select_result = executor().execute("SELECT name, value FROM ddl_test_table WHERE name = 'Test'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
    EXPECT_EQ(select_result.value().get<std::string>(0, 0), "Test");
    EXPECT_EQ(select_result.value().get<int>(0, 1), 42);

    // Drop table using DDL expression
    auto drop_expr   = drop_table(table, true, true);
    auto drop_query  = library().compiler().compile(drop_expr);
    auto drop_result = executor().execute(drop_query);
    ASSERT_TRUE(drop_result.is_success()) << "DROP failed: " << drop_result.error<ErrorContext>();
    EXPECT_FALSE(TableExists("ddl_test_table"));
}

TEST_F(CompiledDdlTest, CreateTableWithAllConstraintTypes) {
    // Create a comprehensive table with multiple constraint types
    auto table = std::make_shared<Table>("ddl_comprehensive_test");
    table->add_field<int>("id", "SERIAL").primary_key("id");
    table->add_field<std::string>("username", "VARCHAR(50)").nullable("username", false).unique("username");
    table->add_field<std::string>("email", "VARCHAR(255)").nullable("email", false).unique("email");
    table->add_field<int>("age", "INTEGER");
    table->add_field<bool>("active", "BOOLEAN");
    table->add_field<std::string>("created_at", "TIMESTAMP");

    // Set defaults
    if (auto* field = table->get_field_schema("active")) {
        field->default_value = "true";
    }
    if (auto* field = table->get_field_schema("created_at")) {
        field->default_value = "CURRENT_TIMESTAMP";
    }

    auto create_expr   = create_table(table, true);
    auto create_query  = library().compiler().compile(create_expr);
    auto create_result = executor().execute(create_query);
    ASSERT_TRUE(create_result.is_success()) << "CREATE failed: " << create_result.error<ErrorContext>();

    EXPECT_TRUE(TableExists("ddl_comprehensive_test"));
    EXPECT_TRUE(HasPrimaryKey("ddl_comprehensive_test"));
    EXPECT_TRUE(ColumnIsNotNull("ddl_comprehensive_test", "username"));
    EXPECT_TRUE(ColumnIsNotNull("ddl_comprehensive_test", "email"));
    EXPECT_TRUE(HasUniqueConstraint("ddl_comprehensive_test", "username"));
    EXPECT_TRUE(HasUniqueConstraint("ddl_comprehensive_test", "email"));

    // Test inserting with defaults
    auto insert_result = executor().execute(
        "INSERT INTO ddl_comprehensive_test (username, email, age) VALUES ('testuser', 'test@test.com', 25)");
    ASSERT_TRUE(insert_result.is_success());

    // Verify defaults were applied
    auto select_result =
        executor().execute("SELECT active, created_at FROM ddl_comprehensive_test WHERE username = 'testuser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
    EXPECT_EQ(select_result.value().get<bool>(0, 0), true);
    // created_at should not be null (CURRENT_TIMESTAMP default)
    EXPECT_TRUE(select_result.value().get_opt<std::string>(0, 1).has_value());

    // Cleanup
    (void)executor().execute("DROP TABLE IF EXISTS ddl_comprehensive_test CASCADE");
}
