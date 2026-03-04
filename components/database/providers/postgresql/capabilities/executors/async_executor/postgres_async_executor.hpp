#pragma once

#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <connection_slot.hpp>
#include <postgres_params.hpp>
#include <process_pg_result.hpp>
#include <query/compiled_dynamic_query.hpp>
#include <query/compiled_static_query.hpp>

namespace demiplane::db::postgres {

    /**
     * @brief Modern async PostgreSQL executor with coroutine support
     *
     * Provides co_await-able query execution using libpq's async API
     * integrated with Boost.Asio. Designed for exclusive connection access
     * (typically acquired from a cylinder).
     *
     * When constructed with a ConnectionSlot, resets the slot on destruction,
     * eliminating the need for a separate scoped wrapper.
     *
     * Usage:
     *   auto exec = AsyncExecutor(conn, co_await asio::this_coro::executor);
     *   auto result = co_await exec.execute("SELECT * FROM users WHERE id = $1", 42);
     *   if (result) { process(result.value()); }
     *
     * Thread safety: NOT thread-safe. Use strand if concurrent access needed.
     * Cancellation: Supports asio cancellation_slot for query cancellation.
     */
    class AsyncExecutor : gears::NonCopyable {
    public:
        using executor_type = boost::asio::any_io_executor;

        /**
         * @brief Construct executor with exclusive connection access (standalone)
         * @param conn PostgreSQL connection (caller retains ownership)
         * @param executor Asio executor for async operations
         *
         * If conn is nullptr, the executor is in empty state (valid() returns false).
         * Otherwise sets connection to non-blocking mode.
         */
        explicit AsyncExecutor(PGconn* conn, executor_type executor);

        /**
         * @brief Construct executor from a cylinder slot (cylinder-managed)
         * @param slot Connection slot acquired from the cylinder
         * @param executor Asio executor for async operations
         */
        AsyncExecutor(ConnectionSlot& slot, executor_type executor);

        ~AsyncExecutor();

        // Movable
        AsyncExecutor(AsyncExecutor&& other) noexcept;
        AsyncExecutor& operator=(AsyncExecutor&& other) noexcept;

        /**
         * @brief Execute simple query
         * @param query SQL query string
         * @return Awaitable result
         */
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(std::string_view query) const;

        /**
         * @brief Execute parameterized query
         * @param query SQL with placeholders ($1, $2, ...)
         * @param params Parameter pack
         * @return Awaitable result
         */
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(std::string_view query, const Params& params) const;

        /**
         * @brief Execute with variadic parameters (convenience)
         * @param query SQL with placeholders
         * @param args Values convertible to FieldValue
         * @return Awaitable result
         *
         * Example: co_await exec.execute("SELECT * FROM t WHERE id = $1", 42);
         */
        template <typename... Args>
            requires(db::IsFieldValueType<Args> && ...)
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(const std::string_view query, Args&&... args) {
            // NOT a coroutine - runs synchronously to completion
            // Temporaries are still alive here

            auto pool = std::make_shared<std::pmr::unsynchronized_pool_resource>();
            ParamSink sink(pool.get());

            (sink.push(FieldValue{std::forward<Args>(args)}), ...);

            // Pass ownership to coroutine (shared_ptrs copied to coroutine frame before initial_suspend)
            return execute_with_resources(query, std::move(pool), sink.native_packet());
        }

        /**
         * @brief Execute with tuple parameters (convenience)
         * @param query SQL with placeholders
         * @param args Values packed in a tuple
         * @return Awaitable result
         *
         * Example: co_await exec.execute("SELECT * FROM t WHERE id = $1", std::tuple{42});
         */
        template <typename... Args>
            requires(db::IsFieldValueType<Args> && ...)
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(const std::string_view query, const std::tuple<Args...>& args) {
            return std::apply([&](const Args&... a) { return execute(query, a...); }, args);
        }

        /**
         * @brief Execute a compiled static query
         * @param query CompiledStaticQuery object (from compile_static())
         * @return Awaitable result
         */
        template <typename... Params>
            requires(db::IsFieldValueType<Params> && ...)
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(const CompiledStaticQuery<Params...>& query) {
            return execute(query.sql(), query.params());
        }

        /**
         * @brief Execute compiled query
         * @param query CompiledQuery object
         * @return Awaitable result
         */
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(const CompiledDynamicQuery& query) const;

        // Accessors
        [[nodiscard]] executor_type get_executor() const noexcept {
            return executor_;
        }
        [[nodiscard]] PGconn* native_handle() const noexcept {
            return conn_;
        }
        [[nodiscard]] bool valid() const noexcept {
            return conn_ != nullptr && socket_ != nullptr;
        }

    private:
        PGconn* conn_;
        executor_type executor_;
        std::unique_ptr<boost::asio::posix::stream_descriptor> socket_;
        std::int32_t cached_socket_fd_ = -1;
        ConnectionSlot* slot_          = nullptr;

        // Core implementation
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute_with_resources(std::string_view query,
                               std::shared_ptr<std::pmr::memory_resource> pool,
                               std::shared_ptr<Params> params) const;

        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute_impl(const char* query, const Params* params) const;

        // Async primitives
        [[nodiscard]] boost::asio::awaitable<std::optional<ErrorContext>> async_flush() const;
        [[nodiscard]] boost::asio::awaitable<std::optional<ErrorContext>> async_consume_until_ready() const;

        // Result collection
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> collect_single_result() const;

        // Validation
        [[nodiscard]] std::optional<ErrorContext> validate_state() const;

        // Shared setup logic
        void setup_connection();
    };

}  // namespace demiplane::db::postgres
