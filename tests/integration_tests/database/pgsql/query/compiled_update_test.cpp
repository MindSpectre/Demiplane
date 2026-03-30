// Compiled UPDATE Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

// Test fixture for compiled UPDATE queries
class CompiledUpdateTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        CreateUsersTable();
        TruncateUsersTable();
    }

    void TearDown() override {
        if (connection()) {
            DropUsersTable();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== Basic UPDATE Tests ==============

TEST_F(CompiledUpdateTest, UpdateSingleColumn) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('Alice', 30, true)"));

    const auto& u       = schemas().users;
    auto compiled_query = compile_query(update(u).set("age", 31).where(u.column<"name">() == std::string{"Alice"}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'Alice'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 31);
}

TEST_F(CompiledUpdateTest, UpdateMultipleColumns) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('Bob', 25, false)"));

    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(update(u).set("age", 26).set("active", true).where(u.column<"name">() == std::string{"Bob"}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT age, active FROM users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 26);
    EXPECT_EQ(block.get<bool>(0, 1), true);
}

TEST_F(CompiledUpdateTest, UpdateWithInitializerList) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('Charlie', 35, true)"));

    const auto& u       = schemas().users;
    auto compiled_query = compile_query(update(u)
                                            .set({
                                                {"age",    36   },
                                                {"active", false}
    })
                                            .where(u.column<"name">() == std::string{"Charlie"}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT age, active FROM users WHERE name = 'Charlie'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 36);
    EXPECT_EQ(block.get<bool>(0, 1), false);
}

// ============== UPDATE with WHERE Conditions ==============

TEST_F(CompiledUpdateTest, UpdateWithSimpleWhere) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User1', 20, true)"));
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User3', 40, true)"));

    const auto& u       = schemas().users;
    auto compiled_query = compile_query(update(u).set("active", false).where(u.column<"age">() > 25));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT COUNT(*) FROM users WHERE active = false");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

TEST_F(CompiledUpdateTest, UpdateWithComplexWhere) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User3', 35, false)"));

    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(update(u).set("age", 40).where((u.column<"age">() >= 25) && (u.column<"active">() == true)));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT COUNT(*) FROM users WHERE age = 40");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

TEST_F(CompiledUpdateTest, UpdateWithOrCondition) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User1', 20, true)"));
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, false)"));
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User3', 40, true)"));

    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(update(u).set("age", 50).where((u.column<"age">() < 25) || (u.column<"age">() > 35)));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT COUNT(*) FROM users WHERE age = 50");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

// ============== UPDATE without WHERE (all rows) ==============

TEST_F(CompiledUpdateTest, UpdateAllRows) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, false)"));
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User3', 35, true)"));

    auto query  = compile_query(update(schemas().users).set("active", true));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT COUNT(*) FROM users WHERE active = true");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 3);
}

// ============== UPDATE with Different Data Types ==============

TEST_F(CompiledUpdateTest, UpdateString) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age) VALUES ('OldName', 30)"));

    const auto& u       = schemas().users;
    auto compiled_query = compile_query(update(u).set("name", std::string{"NewName"}).where(u.column<"age">() == 30));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT name FROM users WHERE age = 30");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<std::string>(0, 0), "NewName");
}

TEST_F(CompiledUpdateTest, UpdateBoolean) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, active) VALUES ('TestUser', true)"));

    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(update(u).set("active", false).where(u.column<"name">() == std::string{"TestUser"}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT active FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<bool>(0, 0), false);
}

TEST_F(CompiledUpdateTest, UpdateInteger) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age) VALUES ('TestUser', 25)"));

    const auto& u       = schemas().users;
    auto compiled_query = compile_query(update(u).set("age", 50).where(u.column<"name">() == std::string{"TestUser"}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 50);
}

// ============== UPDATE with NULL Values ==============

TEST_F(CompiledUpdateTest, UpdateToNull) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age) VALUES ('TestUser', 30)"));

    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(update(u).set("age", std::monostate{}).where(u.column<"name">() == std::string{"TestUser"}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update to NULL failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    auto age_opt = block.get_opt<int>(0, 0);
    EXPECT_FALSE(age_opt.has_value()) << "Age should be NULL";
}

// ============== UPDATE with Table Name String ==============

TEST_F(CompiledUpdateTest, UpdateWithTableName) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age) VALUES ('TestUser', 25)"));

    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(update("users").set("age", 35).where(u.column<"name">() == std::string{"TestUser"}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 35);
}

// ============== UPDATE Edge Cases ==============

TEST_F(CompiledUpdateTest, UpdateNoMatch) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age) VALUES ('TestUser', 25)"));

    const auto& u       = schemas().users;
    auto compiled_query = compile_query(update(u).set("age", 50).where(u.column<"age">() > 100));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 25);  // Original value
}

TEST_F(CompiledUpdateTest, UpdateEmptyTable) {
    const auto& u       = schemas().users;
    auto compiled_query = compile_query(update(u).set("active", false));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT COUNT(*) FROM users");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 0);
}

TEST_F(CompiledUpdateTest, UpdateToSameValue) {
    EXPECT_TRUE(executor().execute("INSERT INTO users (name, age) VALUES ('TestUser', 30)"));

    const auto& u       = schemas().users;
    auto compiled_query = compile_query(update(u).set("age", 30).where(u.column<"name">() == std::string{"TestUser"}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 30);
}
