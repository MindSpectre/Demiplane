// Compiled CASE Expression Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledCaseTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        CreateUsersTable();
        CreateOrdersTable();
        InsertTestUsers();
        InsertTestOrders();
    }

    void TearDown() override {
        if (connection()) {
            DropOrdersTable();
            DropUsersTable();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== Simple CASE Tests ==============

TEST_F(CompiledCaseTest, SimpleCaseWhen) {
    const auto& s    = schemas();
    auto case_active = case_when(s.users.column<"active">() == true, lit("Active"));
    auto query       = compile_query(select(s.users.column<"name">(), case_active.as("status")).from(s.users));
    auto result      = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
    EXPECT_GE(block.cols(), 2);  // name and status columns
}

TEST_F(CompiledCaseTest, CaseWithElse) {
    const auto& s    = schemas();
    auto case_status = case_when(s.users.column<"active">() == true, lit("Active")).else_(lit("Inactive"));
    auto query       = compile_query(select(s.users.column<"name">(), case_status.as("status")).from(s.users));
    auto result      = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledCaseTest, CaseMultipleWhen) {
    const auto& s     = schemas();
    auto age_category = case_when(s.users.column<"age">() < lit(25), lit("Young"))
                            .when(s.users.column<"age">() < lit(40), lit("Adult"))
                            .when(s.users.column<"age">() < lit(60), lit("Middle-aged"))
                            .else_(lit("Senior"));
    auto query = compile_query(
        select(s.users.column<"name">(), s.users.column<"age">(), age_category.as("age_group")).from(s.users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
    EXPECT_GE(block.cols(), 3);  // name, age, age_group
}

TEST_F(CompiledCaseTest, CaseInSelect) {
    const auto& s   = schemas();
    auto order_size = case_when(s.orders.column<"amount">() < lit(100.0), lit("Small"))
                          .when(s.orders.column<"amount">() < lit(500.0), lit("Medium"))
                          .else_(lit("Large"));
    auto query = compile_query(select(s.orders.column<"id">(), s.orders.column<"amount">(), order_size.as("order_size"))
                                   .from(s.orders)
                                   .where(s.orders.column<"completed">() == true));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return orders with size category
    EXPECT_GE(block.cols(), 3);  // id, amount, order_size
}

TEST_F(CompiledCaseTest, CaseWithComparison) {
    const auto& s = schemas();
    auto priority = case_when(s.orders.column<"amount">() > lit(1000.0), lit(1))
                        .when(s.orders.column<"amount">() > lit(500.0), lit(2))
                        .when(s.orders.column<"amount">() > lit(100.0), lit(3))
                        .else_(lit(4));
    auto query = compile_query(
        select(s.orders.column<"id">(), s.orders.column<"amount">(), priority.as("priority")).from(s.orders));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledCaseTest, CaseNested) {
    const auto& s   = schemas();
    auto high_value = case_when(s.users.column<"active">() == true,
                                case_when(s.users.column<"age">() > lit(30), lit("VIP")).else_(lit("Regular")))
                          .else_(lit("Inactive"));
    auto query  = compile_query(select(s.users.column<"name">(), high_value.as("customer_type")).from(s.users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}
