// Compiled CASE Expression Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

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
    auto query  = library().produce<case_expr::SimpleCaseWhen>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
    EXPECT_GE(block.cols(), 2);  // name and status columns
}

TEST_F(CompiledCaseTest, CaseWithElse) {
    auto query  = library().produce<case_expr::CaseWithElse>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledCaseTest, CaseMultipleWhen) {
    auto query  = library().produce<case_expr::CaseMultipleWhen>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
    EXPECT_GE(block.cols(), 3);  // name, age, age_group
}

TEST_F(CompiledCaseTest, CaseInSelect) {
    auto query  = library().produce<case_expr::CaseInSelect>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return orders with size category
    EXPECT_GE(block.cols(), 3);  // id, amount, order_size
}

TEST_F(CompiledCaseTest, CaseWithComparison) {
    auto query  = library().produce<case_expr::CaseWithComparison>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledCaseTest, CaseNested) {
    auto query  = library().produce<case_expr::CaseNested>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}
