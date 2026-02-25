#pragma once

#include <gears_class_traits.hpp>
#include <gears_outcome.hpp>
#include <postgres_async_executor.hpp>
#include <postgres_errors.hpp>
#include <postgres_sync_executor.hpp>
#include <postgres_transaction.hpp>

#include "cylinder/cylinder.hpp"

namespace demiplane::db::postgres {

    /**
     * @brief PostgreSQL session owning a connection cylinder and background janitor
     *
     * The Session is the primary entry point for database operations.
     * Each with_sync() / with_async() call acquires a slot from the cylinder
     * and returns a slot-aware executor that resets the slot on destruction.
     *
     * Usage:
     *   auto session = Session(conn_config, cylinder_config);
     *
     *   // Synchronous
     *   auto result = session.with_sync().execute("SELECT 1");
     *
     *   // Asynchronous
     *   auto result = co_await session.with_async(io_exec).execute("SELECT 1");
     */
    class Session : gears::Immutable {
    public:
        using executor_type = boost::asio::any_io_executor;

        Session(ConnectionConfig connection_config, CylinderConfig cylinder_config);
        ~Session();

        /**
         * @brief Get a synchronous executor with an acquired connection
         * @return SyncExecutor that auto-releases its slot on destruction
         */
        [[nodiscard]] SyncExecutor with_sync();

        /**
         * @brief Get an asynchronous executor with an acquired connection
         * @param exec Boost.Asio executor for async I/O
         * @return AsyncExecutor that auto-releases its slot on destruction
         */
        [[nodiscard]] AsyncExecutor with_async(executor_type exec);

        /**
         * @brief Shutdown the session: stop janitor, drain cylinder
         */
        void shutdown();

        // ============== Transaction Factories ==============

        [[nodiscard]] gears::Outcome<Transaction, ErrorContext> begin_transaction(TransactionOptions opts = {});

        [[nodiscard]] gears::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts = {});

        // ============== Cylinder Stats ==============

        [[nodiscard]] std::size_t cylinder_capacity() const noexcept;

        [[nodiscard]] std::size_t cylinder_active_count() const noexcept;

        [[nodiscard]] std::size_t cylinder_free_count() const noexcept;

        [[nodiscard]] bool is_shutdown() const noexcept;

    private:
        ConnectionCylinder cylinder_;
        CylinderJanitor janitor_;
    };

}  // namespace demiplane::db::postgres
