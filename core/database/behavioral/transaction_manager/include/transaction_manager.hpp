#pragma once
#include <functional>
#include <json/json.h>
#include <memory>
#include <mutex>
#include <utility>

#include "db_interface.hpp"

namespace demiplane::database {
    class TransactionManager {
    public:
        // Default Constructor
        TransactionManager() = default;

        // Move Constructor and Move Assignment Operator
        TransactionManager(TransactionManager&& other) noexcept : tables_(std::move(other.tables_)) {}

        TransactionManager& operator=(TransactionManager&& other) noexcept {
            if (this != &other) {
                std::lock_guard lock(internalMutex);
                tables_ = std::move(other.tables_);
            }
            return *this;
        }

        // Adds a new table with its associated database connection and mutex
        void add_table(const std::string& tableName, std::unique_ptr<TransactionTrait> dbConnection,
            std::unique_ptr<std::recursive_mutex> tableMutex) {
            std::lock_guard lock(internalMutex);
            TableStatus nw_table_status;
            nw_table_status.connection = std::move(dbConnection);
            nw_table_status.mutex      = std::move(tableMutex);
            tables_.insert(std::make_pair(tableName, std::move(nw_table_status)));
        }

        // Removes a table and its associated connection and mutex
        void remove_table(const std::string& tableName) {
            std::lock_guard lock(internalMutex);
            tables_.erase(tableName);
        }

        // Starts transactions on multiple tables
        template <typename... Args>
        void start_transaction(Args... tableNames) {
            std::lock_guard lock(internalMutex);
            (start_transaction_impl(tableNames), ...);
        }

        // Commits transactions on multiple tables
        template <typename... Args>
        void commit_transaction(Args... tableNames) {
            std::lock_guard lock(internalMutex);
            (commit_transaction_impl(tableNames), ...);
        }

        // Rolls back transactions on multiple tables
        template <typename... Args>
        void rollback_transaction(Args... tableNames) {
            std::lock_guard lock(internalMutex);
            (rollback_transaction_impl(tableNames), ...);
        }

    private:
        struct TableStatus {
            std::unique_ptr<TransactionTrait> connection;
            std::unique_ptr<std::recursive_mutex> mutex;
            bool transaction_started{};
        };

        std::unordered_map<std::string, TableStatus> tables_;
        std::mutex internalMutex; // Protects the access to the maps

        // Starts a transaction on a specific table
        void start_transaction_impl(const std::string& tableName) {
            check_and_start_table_transaction(tableName);
            lock_table(tableName);
        }

        // Commits a transaction on a specific table
        void commit_transaction_impl(const std::string& tableName) {
            check_table_transaction_in_progress(tableName);
            unlock_table(tableName);
            auto& [connection, mutex, transaction_started] = tables_.at(tableName);
            connection->commit_transaction();
            transaction_started = false;
        }

        // Rolls back a transaction on a specific table
        void rollback_transaction_impl(const std::string& tableName) {
            check_table_transaction_in_progress(tableName);
            unlock_table(tableName);
            auto& [connection, mutex, transaction_started] = tables_.at(tableName);
            connection->rollback_transaction();
            transaction_started = false;
        }

        // Locks the table-specific mutex
        void lock_table(const std::string& tableName) {
            if (const auto tableMutex = get_table_mutex(tableName)) {
                if (const auto& mutexPtr = tableMutex->get()) {
                    mutexPtr->lock();
                }
            }
        }

        // Unlocks the table-specific mutex
        void unlock_table(const std::string& tableName) {
            if (const auto tableMutex = get_table_mutex(tableName)) {
                if (const auto& mutexPtr = tableMutex->get()) {
                    mutexPtr->unlock();
                }
            }
        }

        std::optional<std::reference_wrapper<std::unique_ptr<std::recursive_mutex>>> get_table_mutex(
            const std::string& tableName) {
            if (auto it = tables_.find(tableName); it != tables_.end()) {
                return it->second.mutex;
            }
            return std::nullopt;
        }

        // Starts a transaction for a specific table, checking if already in progress
        void check_and_start_table_transaction(const std::string& tableName) {
            try {
                auto& [connection, mutex, transaction_started] = tables_.at(tableName);
                if (transaction_started) {
                    throw std::runtime_error("Transaction is already in progress for table: " + tableName);
                }
                connection->start_transaction();
                transaction_started = true;
            } catch (const std::out_of_range&) {
                throw std::runtime_error("Table is not found: " + tableName);
            }
        }

        // Checks if a transaction is in progress for a specific table
        void check_table_transaction_in_progress(const std::string& tableName) {
            try {
                if (auto& [connection, mutex, transaction_started] = tables_.at(tableName); !transaction_started) {
                    throw std::runtime_error("No transaction in progress for table: " + tableName);
                }
            } catch (const std::out_of_range&) {
                throw std::runtime_error("Table is not found: " + tableName);
            }
        }
    };
} // namespace demiplane::database
