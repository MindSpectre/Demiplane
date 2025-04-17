#include <gtest/gtest.h>
#include <memory>

#include "db_factory.hpp"
#include "transaction_manager.hpp"

class TransactionManagerTest : public testing::Test
{
protected:
    // The TransactionManager instance to be tested
    demiplane::database::TransactionManager transaction_manager;

    // Test table names
    std::string table1 = "table1";
    std::string table2 = "table2";
    demiplane::database::ConnectParams connect_params;
    // Shared pointers for mock database connections and mutexes
    std::unique_ptr<demiplane::database::BasicMockDbClient> mock_db1;
    std::unique_ptr<demiplane::database::BasicMockDbClient> mock_db2;
    std::unique_ptr<std::recursive_mutex> mutex1;
    std::unique_ptr<std::recursive_mutex> mutex2;

    void SetUp() override
    {
        connect_params = demiplane::database::ConnectParams("123.123.123.123", 23, "mock_db1.db", "0.0.0.0", "123133");
        // Initialize mocks and mutexes
        mock_db1 = demiplane::database::creational::DatabaseFactory::create_basic_mock_database();
        mock_db2 = demiplane::database::creational::DatabaseFactory::create_basic_mock_database(connect_params);
        mutex1 = std::make_unique<std::recursive_mutex>();
        mutex2 = std::make_unique<std::recursive_mutex>();
        // Add tables to TransactionManager
        transaction_manager.add_table(table1, std::move(mock_db1), std::move(mutex1));
        transaction_manager.add_table(table2, std::move(mock_db2), std::move(mutex2));
    }
};

TEST_F(TransactionManagerTest, AddTable)
{
    // Verify that tables have been added
    EXPECT_NO_THROW(transaction_manager.start_transaction(table1));
    EXPECT_NO_THROW(transaction_manager.start_transaction(table2));
}

TEST_F(TransactionManagerTest, RemoveTable)
{
    // Remove table1 and verify it has been removed
    transaction_manager.remove_table(table1);
    EXPECT_THROW(transaction_manager.start_transaction(table1), std::runtime_error);
}

TEST_F(TransactionManagerTest, StartTransaction)
{
    // Start a transaction on table1
    EXPECT_NO_THROW(transaction_manager.start_transaction(table1));
}

TEST_F(TransactionManagerTest, CommitTransaction)
{
    EXPECT_NO_THROW(transaction_manager.start_transaction(table1));
    EXPECT_NO_THROW(transaction_manager.commit_transaction(table1));
}

TEST_F(TransactionManagerTest, RollbackTransaction)
{
    EXPECT_NO_THROW(transaction_manager.start_transaction(table1));
    EXPECT_NO_THROW(transaction_manager.rollback_transaction(table1));
}

TEST_F(TransactionManagerTest, TransactionAlreadyInProgress)
{
    EXPECT_NO_THROW(transaction_manager.start_transaction(table1));
    EXPECT_THROW(transaction_manager.start_transaction(table1), std::runtime_error);
}
