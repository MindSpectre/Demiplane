#pragma once

#include <memory>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <compiled_query/compiled_dynamic_query.hpp>
#include <compiled_query/compiled_static_query.hpp>
#include <connection_holder.hpp>
#include <executor_concept.hpp>
#include <gears_concepts.hpp>
#include <postgres_params.hpp>
#include <process_pgresult.hpp>

namespace demiplane::db::postgres {

    /**
     * @brief Modern async PostgreSQL executor with coroutine support
     *
     * Provides co_await-able query execution using libpq's async API
     * integrated with Boost.Asio. Designed for exclusive connection access
     * (typically acquired from a pool).
     *
     * When constructed with a ConnectionSlot, resets the slot on destruction,
     * eliminating the need for a separate scoped wrapper.
     *
     * @details
     * Usage:\n
     *   auto exec = AsyncExecutor(conn, co_await asio::this_coro::executor);\n
     *   auto result = co_await exec.execute("SELECT * FROM users WHERE id = $1", 42);\n
     *   if (result) { process(result.value()); }\n
     *
     * Thread safety: NOT thread-safe. Use strand if concurrent access needed.
     * Cancellation: Supports asio cancellation_slot for query cancellation.
     */
    class AsyncExecutor : gears::NonCopyable {
    public:
        /**
         * @brief Construct executor with exclusive connection access (standalone)
         * @param conn PostgreSQL connection (caller retains ownership)
         * @param executor Asio executor for async operations
         *
         * If conn is nullptr, the executor is in empty state (valid() returns false).
         * Otherwise sets connection to non-blocking mode.
         */
        explicit AsyncExecutor(PGconn* conn, boost::asio::any_io_executor executor);

        /**
         * @brief Construct executor from a pool-managed connection holder
         * @param holder Weak reference to a ConnectionHolder owned by the pool.
         * @param executor Asio executor for async operations
         */
        AsyncExecutor(std::weak_ptr<ConnectionHolder> holder, boost::asio::any_io_executor executor);

        ~AsyncExecutor();

        // Movable
        AsyncExecutor(AsyncExecutor&& other) noexcept;

        AsyncExecutor& operator=(AsyncExecutor&& other) noexcept;

        [[nodiscard]] bool valid() const noexcept {
            return conn_ != nullptr && socket_ != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return valid();
        }

        /**
         * @brief Set cleanup SQL to run when this executor releases the slot
         * @param query Predefined cleanup query
         * @return *this for chaining: session->with_async(exec).do_cleanup(CleanupQuery::DeallocateAll)
         */
        template <typename Self>
        constexpr auto&& do_cleanup(this Self&& self, const CleanupQuery query) noexcept {
            if (auto live = self.holder_.lock()) {
                live->set_cleanup(query);
            }
            return std::forward<Self>(self);
        }
        /**
         * @brief Execute simple query
         * @param query SQL query string (must be null-terminated: std::string, pmr::string, const char*)
         * @return Awaitable result
         */
        template <gears::IsNullTerminatedString StringQueryT>
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(const StringQueryT& query) const {
            co_return co_await execute_impl(gears::as_c_str(query), nullptr);
        }

        /**
         * @brief Execute parameterized query
         * @param query SQL with placeholders ($1, $2, ...)
         * @param params Parameter pack
         * @return Awaitable result
         */
        template <gears::IsNullTerminatedString StringQueryT>
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(const StringQueryT& query, const Params& params) const {
            co_return co_await execute_impl(gears::as_c_str(query), &params);
        }

        /**
         * @brief Execute with variadic parameters (convenience)
         * @param query SQL with placeholders
         * @param args Values convertible to FieldValue
         * @return Awaitable result
         *
         * Parameters are taken by value because Boost.Asio awaitables are lazy
         * (suspend at initial_suspend). Reference parameters would dangle before
         * the coroutine body runs.
         *
         * Example: co_await exec.execute("SELECT * FROM t WHERE id = $1", 42);
         */
        template <gears::IsNullTerminatedString StringQueryT, typename... Args>
            requires(db::IsFieldValueType<std::remove_cvref_t<Args>> && ...)
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>> execute(StringQueryT query,
                                                                                                Args... args) const {
            std::array<std::byte, 2048> stack_buffer{};
            std::pmr::monotonic_buffer_resource pool{stack_buffer.data(), stack_buffer.size()};

            ParamSink sink(&pool);

            (sink.push(FieldValue{std::move(args)}), ...);

            const auto params = sink.native_packet();
            co_return co_await execute_impl(gears::as_c_str(query), params.get());
        }

        /**
         * @brief Execute with tuple parameters (convenience)
         * @param query SQL with placeholders
         * @param args Values packed in a tuple
         * @return Awaitable result
         *
         * Tuple is taken by value for coroutine safety (see variadic overload).
         *
         * Example: co_await exec.execute("SELECT * FROM t WHERE id = $1", std::tuple{42});
         */
        template <gears::IsNullTerminatedString StringQueryT, typename... Args>
            requires(db::IsFieldValueType<Args> && ...)
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(StringQueryT query, std::tuple<Args...> args) const {
            co_return co_await std::apply([&](Args&... a) { return execute(std::move(query), std::move(a)...); }, args);
        }

        /**
         * @brief Execute a compiled static query
         * @param query CompiledStaticQuery object (from compile_static())
         * @return Awaitable result
         *
         * Query is taken by value for coroutine safety (see variadic overload).
         */
        template <typename SqlStringT, typename... Params>
            requires(db::IsFieldValueType<Params> && ...)
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(CompiledStaticQuery<SqlStringT, Params...> query) const {
            co_return co_await execute(query.c_sql(), query.params());
        }

        /**
         * @brief Execute compiled query
         * @param query CompiledQuery object
         * @return Awaitable result
         */
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
        execute(const CompiledDynamicQuery& query) const;

        // Accessors
        [[nodiscard]] constexpr boost::asio::any_io_executor get_executor() const noexcept {
            return executor_;
        }

        [[nodiscard]] constexpr PGconn* native_handle() const noexcept {
            return conn_;
        }

    private:
        PGconn* conn_ = nullptr;
        boost::asio::any_io_executor executor_;
        std::unique_ptr<boost::asio::posix::stream_descriptor> socket_;
        std::int32_t cached_socket_fd_ = -1;
        std::weak_ptr<ConnectionHolder> holder_;

        // Core implementation
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

    static_assert(IsExecutor<AsyncExecutor>);

}  // namespace demiplane::db::postgres
