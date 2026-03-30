// Unit tests for StaticTable<StaticFieldSchema...> with query expressions
// Verifies that IsStaticTable, IsTable, and from() work with the query builder

#include <string>

#include <db_table.hpp>
#include <gtest/gtest.h>
#include <postgres_dialect.hpp>
#include <postgres_type_mapping.hpp>
#include <query_compiler.hpp>

using namespace demiplane::db;
using namespace demiplane::db::constraints;
using namespace demiplane::db::postgres;

// Define static table types for tests
using UsersTable = StaticTable<"users",
                               StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                               StaticFieldSchema<std::string, "name">,
                               StaticFieldSchema<int, "age">,
                               StaticFieldSchema<bool, "active">>;

using PostsTable = StaticTable<"posts",
                               StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                               StaticFieldSchema<int, "user_id">,
                               StaticFieldSchema<std::string, "title">,
                               StaticFieldSchema<bool, "published">>;

// ============== Concept Tests ==============

TEST(StaticTableQueryTest, StaticTableSatisfiesIsStaticTable) {
    static_assert(IsStaticTable<UsersTable>, "UsersTable must satisfy IsStaticTable");
    static_assert(IsStaticTable<PostsTable>, "PostsTable must satisfy IsStaticTable");
}

TEST(StaticTableQueryTest, StaticTableSatisfiesIsTable) {
    static_assert(IsTable<UsersTable>, "UsersTable must satisfy IsTable");
    static_assert(IsTable<PostsTable>, "PostsTable must satisfy IsTable");
}

TEST(StaticTableQueryTest, DynamicTablePtrStillSatisfiesIsTable) {
    static_assert(IsTable<DynamicTablePtr>, "DynamicTablePtr must still satisfy IsTable");
}

TEST(StaticTableQueryTest, StringStillSatisfiesIsTable) {
    static_assert(IsTable<std::string>, "std::string must still satisfy IsTable");
    static_assert(IsTable<std::string_view>, "std::string_view must still satisfy IsTable");
}

// ============== SELECT FROM Tests ==============

TEST(StaticTableQueryTest, SelectFromStaticTable) {
    auto users = UsersTable{};

    auto q = select(users.column<"id">(), users.column<"name">()).from(users);

    QueryCompiler<PostgresDialect> compiler;
    auto result = compiler.compile_dynamic<ParamMode::Inline>(q);
    auto sql    = std::string(result.sql());

    EXPECT_NE(sql.find("SELECT"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("FROM"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"users\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"id\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"name\""), std::string::npos) << "SQL: " << sql;
}

TEST(StaticTableQueryTest, SelectAllColumnsFromStaticTable) {
    auto users = UsersTable{};

    auto q = select(users.column<"id">(), users.column<"name">(), users.column<"age">(), users.column<"active">())
                 .from(users);

    QueryCompiler<PostgresDialect> compiler;
    auto result = compiler.compile_dynamic<ParamMode::Inline>(q);
    auto sql    = std::string(result.sql());

    EXPECT_NE(sql.find("\"id\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"name\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"age\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"active\""), std::string::npos) << "SQL: " << sql;
}

TEST(StaticTableQueryTest, SelectFromStaticTableWithWhere) {
    auto users = UsersTable{};

    auto id_col = users.column<"id">();
    auto q      = select(id_col, users.column<"name">()).from(users).where(id_col == lit(1));

    QueryCompiler<PostgresDialect> compiler;
    auto result = compiler.compile_dynamic<ParamMode::Inline>(q);
    auto sql    = std::string(result.sql());

    EXPECT_NE(sql.find("SELECT"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("FROM"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("WHERE"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"users\""), std::string::npos) << "SQL: " << sql;
}

// ============== DELETE FROM Tests ==============

TEST(StaticTableQueryTest, DeleteFromStaticTable) {
    auto users = UsersTable{};

    auto q = delete_from(users);

    QueryCompiler<PostgresDialect> compiler;
    auto result = compiler.compile_dynamic<ParamMode::Inline>(q);
    auto sql    = std::string(result.sql());

    EXPECT_NE(sql.find("DELETE FROM"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"users\""), std::string::npos) << "SQL: " << sql;
}

TEST(StaticTableQueryTest, DeleteFromStaticTableWithWhere) {
    auto users = UsersTable{};

    auto id_col = users.column<"id">();
    auto q      = delete_from(users).where(id_col == lit(42));

    QueryCompiler<PostgresDialect> compiler;
    auto result = compiler.compile_dynamic<ParamMode::Inline>(q);
    auto sql    = std::string(result.sql());

    EXPECT_NE(sql.find("DELETE FROM"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"users\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("WHERE"), std::string::npos) << "SQL: " << sql;
}

// ============== DDL Tests ==============

TEST(StaticTableQueryTest, CreateTableFromStaticTable) {
    auto users = UsersTable{};

    auto q = create_table(users);

    QueryCompiler<PostgresDialect> compiler;
    auto result = compiler.compile_dynamic<ParamMode::Inline>(q);
    auto sql    = std::string(result.sql());

    EXPECT_NE(sql.find("CREATE TABLE"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"users\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"id\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("PRIMARY KEY"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"name\""), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("\"age\""), std::string::npos) << "SQL: " << sql;
}

TEST(StaticTableQueryTest, CreateTableIfNotExists) {
    auto users = UsersTable{};

    auto q = create_table(users, true);

    QueryCompiler<PostgresDialect> compiler;
    auto result = compiler.compile_dynamic<ParamMode::Inline>(q);
    auto sql    = std::string(result.sql());

    EXPECT_NE(sql.find("IF NOT EXISTS"), std::string::npos) << "SQL: " << sql;
}

TEST(StaticTableQueryTest, AggregateWithStaticColumn) {
    auto users = UsersTable{};

    auto c = count(users.column<"id">());
    auto s = sum(users.column<"age">());
    // Verify they compile — these are expression objects
    (void)c;
    (void)s;
    SUCCEED();
}

TEST(StaticTableQueryTest, OrderByWithStaticColumn) {
    auto users = UsersTable{};

    auto q = select(users.column<"id">(), users.column<"name">())
                 .from(users)
                 .order_by(asc(users.column<"name">()), desc(users.column<"age">()));

    QueryCompiler<PostgresDialect> compiler;
    auto result = compiler.compile_dynamic<ParamMode::Inline>(q);
    auto sql    = std::string(result.sql());

    EXPECT_NE(sql.find("ORDER BY"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("ASC"), std::string::npos) << "SQL: " << sql;
    EXPECT_NE(sql.find("DESC"), std::string::npos) << "SQL: " << sql;
}
