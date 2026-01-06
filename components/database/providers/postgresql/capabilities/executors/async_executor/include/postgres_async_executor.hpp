#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <compiled_query.hpp>
#include <postgres_executor.hpp>
#include <postgres_params.hpp>

namespace demiplane::db::postgres {

    /**
     * @brief Modern async PostgreSQL executor with coroutine support
     *
     * Provides co_await-able query execution using libpq's async API
     * integrated with Boost.Asio. Designed for exclusive connection access
     * (typically acquired from a pool).
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
         * @brief Construct executor with exclusive connection access
         * @param conn PostgreSQL connection (must be valid, caller retains ownership)
         * @param executor Asio executor for async operations
         * @throws std::runtime_error if connection invalid or socket unavailable
         *
         * Sets connection to non-blocking mode. Connection must remain valid
         * for executor lifetime.result_type
         */
        explicit AsyncExecutor(PGconn* conn, executor_type executor);

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
         * @brief Execute compiled query
         * @param query CompiledQuery object
         * @return Awaitable result
         */
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(const CompiledQuery& query) const;

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
        std::int32_t cached_socket_fd_ = -1;  // For detecting reconnection

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
    };

}  // namespace demiplane::db::postgres
