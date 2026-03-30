// Compiled CLAUSE Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledClauseTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        CreateClauseTestTables();
        InsertClauseTestData();
    }

    void TearDown() override {
        if (connection()) {
            DropClauseTestTables();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== FROM Clause Tests ==============

TEST_F(CompiledClauseTest, FromTable) {
    auto query  = compile_query(select(schemas().users.column<"name">()).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, FromTableName) {
    auto query  = compile_query(select(1).from("test_table"));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== WHERE Clause Tests ==============

TEST_F(CompiledClauseTest, WhereSimple) {
    auto query  = compile_query(select(schemas().users.column<"name">())
                                   .from(schemas().users)
                                   .where(schemas().users.column<"active">() == true));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return only active users
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, WhereComplex) {
    auto query  = compile_query(select(schemas().users.column<"name">())
                                   .from(schemas().users_extended)
                                   .where(schemas().users_extended.column<"age">() > 18 &&
                                          (schemas().users_extended.column<"active">() == true ||
                                           schemas().users_extended.column<"salary">() > lit(50000.0))));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, WhereIn) {
    auto query  = compile_query(select(schemas().users.column<"name">())
                                   .from(schemas().users)
                                   .where(in(schemas().users.column<"age">(), 25, 30, 35)));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, WhereBetween) {
    auto query =
        compile_query(select(schemas().users_extended.column<"name">())
                          .from(schemas().users_extended)
                          .where(between(schemas().users_extended.column<"salary">(), lit(30000.0), lit(80000.0))));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== GROUP BY Clause Tests ==============

TEST_F(CompiledClauseTest, GroupBySingle) {
    auto query  = compile_query(select(schemas().users_extended.column<"department">(),
                                      count(schemas().users_extended.column<"id">()).as("count"))
                                   .from(schemas().users_extended)
                                   .group_by(schemas().users_extended.column<"department">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should have groups for each department
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, GroupByMultiple) {
    auto query = compile_query(
        select(schemas().users_extended.column<"department">(),
               schemas().users_extended.column<"active">(),
               count(schemas().users_extended.column<"id">()).as("count"))
            .from(schemas().users_extended)
            .group_by(schemas().users_extended.column<"department">(), schemas().users_extended.column<"active">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, GroupByWithWhere) {
    auto query  = compile_query(select(schemas().users_extended.column<"department">(),
                                      avg(schemas().users_extended.column<"salary">()).as("avg_salary"))
                                   .from(schemas().users_extended)
                                   .where(schemas().users_extended.column<"active">() == true)
                                   .group_by(schemas().users_extended.column<"department">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== HAVING Clause Tests ==============

TEST_F(CompiledClauseTest, HavingSimple) {
    auto query  = compile_query(select(schemas().users_extended.column<"department">(),
                                      count(schemas().users_extended.column<"id">()).as("count"))
                                   .from(schemas().users_extended)
                                   .group_by(schemas().users_extended.column<"department">())
                                   .having(count(schemas().users_extended.column<"id">()) > 5));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, HavingMultiple) {
    auto query  = compile_query(select(schemas().users_extended.column<"department">(),
                                      avg(schemas().users_extended.column<"salary">()).as("avg_salary"),
                                      count(schemas().users_extended.column<"id">()).as("count"))
                                   .from(schemas().users_extended)
                                   .group_by(schemas().users_extended.column<"department">())
                                   .having(count(schemas().users_extended.column<"id">()) > 3 &&
                                           avg(schemas().users_extended.column<"salary">()) > lit(45000.0)));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, HavingWithWhere) {
    auto query  = compile_query(select(schemas().users_extended.column<"department">(),
                                      max(schemas().users_extended.column<"salary">()).as("max_salary"))
                                   .from(schemas().users_extended)
                                   .where(schemas().users_extended.column<"active">() == true)
                                   .group_by(schemas().users_extended.column<"department">())
                                   .having(max(schemas().users_extended.column<"salary">()) > lit(70000.0)));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== ORDER BY Clause Tests ==============

TEST_F(CompiledClauseTest, OrderByAsc) {
    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().users.column<"age">())
                                   .from(schemas().users)
                                   .order_by(asc(schemas().users.column<"name">())));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, OrderByDesc) {
    auto query =
        compile_query(select(schemas().users_extended.column<"name">(), schemas().users_extended.column<"salary">())
                          .from(schemas().users_extended)
                          .order_by(desc(schemas().users_extended.column<"salary">())));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, OrderByMultiple) {
    auto query  = compile_query(select(schemas().users_extended.column<"name">(),
                                      schemas().users_extended.column<"department">(),
                                      schemas().users_extended.column<"salary">())
                                   .from(schemas().users_extended)
                                   .order_by(asc(schemas().users_extended.column<"department">()),
                                             desc(schemas().users_extended.column<"salary">()),
                                             asc(schemas().users_extended.column<"name">())));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== LIMIT Clause Tests ==============

TEST_F(CompiledClauseTest, LimitBasic) {
    auto query  = compile_query(select(schemas().users.column<"name">()).from(schemas().users).limit(10));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 10);
}

TEST_F(CompiledClauseTest, LimitWithOrderBy) {
    auto query =
        compile_query(select(schemas().users_extended.column<"name">(), schemas().users_extended.column<"salary">())
                          .from(schemas().users_extended)
                          .order_by(desc(schemas().users_extended.column<"salary">()))
                          .limit(5));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 5);
}

TEST_F(CompiledClauseTest, LimitWithWhereOrderBy) {
    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().users.column<"age">())
                                   .from(schemas().users)
                                   .where(schemas().users.column<"active">() == true)
                                   .order_by(asc(schemas().users.column<"age">()))
                                   .limit(20));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 20);
}

// ============== Complex Combined Clause Tests ==============

TEST_F(CompiledClauseTest, ComplexAllClauses) {
    auto query  = compile_query(select(schemas().users_extended.column<"department">(),
                                      count(schemas().users_extended.column<"id">()).as("employee_count"),
                                      avg(schemas().users_extended.column<"salary">()).as("avg_salary"),
                                      max(schemas().users_extended.column<"salary">()).as("max_salary"))
                                   .from(schemas().users_extended)
                                   .where(schemas().users_extended.column<"active">() == true &&
                                          schemas().users_extended.column<"age">() >= 21)
                                   .group_by(schemas().users_extended.column<"department">())
                                   .having(count(schemas().users_extended.column<"id">()) >= 3 &&
                                           avg(schemas().users_extended.column<"salary">()) > lit(40000.0))
                                   .order_by(asc(schemas().users_extended.column<"department">()))
                                   .limit(10));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, ClausesWithJoins) {
    auto query =
        compile_query(select(schemas().users_extended.column<"name">(),
                             schemas().users_extended.column<"department">(),
                             sum(schemas().orders_extended.column<"amount">()).as("total_orders"))
                          .from(schemas().users_extended)
                          .join(schemas().orders_extended)
                          .on(schemas().orders_extended.column<"user_id">() == schemas().users_extended.column<"id">())
                          .where(schemas().users_extended.column<"active">() == true &&
                                 schemas().orders_extended.column<"status">() == "completed")
                          .group_by(schemas().users_extended.column<"id">(),
                                    schemas().users_extended.column<"name">(),
                                    schemas().users_extended.column<"department">())
                          .having(sum(schemas().orders_extended.column<"amount">()) > lit(1000.0))
                          .order_by(desc(schemas().users_extended.column<"name">()))
                          .limit(5));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
