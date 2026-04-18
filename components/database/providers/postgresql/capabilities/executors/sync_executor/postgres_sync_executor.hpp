#pragma once

#include <demiplane/scroll>
#include <memory>
#include <utility>

#include <compiled_query/compiled_dynamic_query.hpp>
#include <compiled_query/compiled_static_query.hpp>
#include <connection_holder.hpp>
#include <executor_concept.hpp>
#include <gears_concepts.hpp>
#include <postgres_params.hpp>
#include <process_pgresult.hpp>

namespace demiplane::db::postgres {

    /**
     * @brief Synchronous PostgreSQL query executor
     *
     * Provides blocking query execution using libpq's synchronous API.
     * Uses ErrorContext for rich error information including SQLSTATE,
     * error messages, hints, and context.
     *
     * When constructed with a ConnectionSlot, acts as a move-only RAII
     * wrapper that resets the slot on destruction.
     */
    class SyncExecutor : gears::NonCopyable {
    public:
        /**
         * @brief Construct a sync executor with a PostgreSQL connection (standalone)
         * @param conn PostgreSQL connection (must be valid and connected)
         */
        explicit SyncExecutor(PGconn* conn) noexcept;

        /**
         * @brief Construct a sync executor from a pool-managed connection holder
         * @param holder Weak reference to a ConnectionHolder owned by the pool.
         *               The holder is locked once at construction to cache the
         *               underlying PGconn for the executor's lifetime.
         */
        explicit SyncExecutor(std::weak_ptr<ConnectionHolder> holder) noexcept;

        ~SyncExecutor();

        // Move-only
        SyncExecutor(SyncExecutor&& other) noexcept;

        SyncExecutor& operator=(SyncExecutor&& other) noexcept;

        [[nodiscard]] bool valid() const noexcept {
            return conn_ != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return valid();
        }

        /**
         * @brief Set cleanup SQL to run when this executor releases the slot
         * @param query Predefined cleanup query
         * @return *this for chaining: session->with_sync().do_cleanup(CleanupQuery::DeallocateAll)
         */
        template <typename Self>
        constexpr auto&& do_cleanup(this Self&& self, const CleanupQuery query) noexcept {
            if (auto live = self.holder_.lock()) {
                live->set_cleanup(query);
            }
            return std::forward<Self>(self);
        }

        /**
         * @brief Execute a simple query without parameters
         * @param query SQL query string (must be null-terminated: std::string, pmr::string, const char*)
         * @return ResultBlock on success, ErrorContext on failure
         */
        template <gears::IsNullTerminatedString QueryT>
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(const QueryT& query) const {
            return execute_impl(gears::as_c_str(query), nullptr);
        }

        /**
         * @brief Execute a query with parameters
         * @param query SQL query string with placeholders ($1, $2, etc.)
         * @param params Parameter values
         * @return ResultBlock on success, ErrorContext on failure
         */
        template <gears::IsNullTerminatedString QueryT>
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(const QueryT& query,
                                                                        const Params& params) const {
            return execute_impl(gears::as_c_str(query), &params);
        }

        /**
         * @brief Execute a query with variadic parameters (convenience overload)
         * @tparam Args Types convertible to FieldValue (bool, int32_t, int64_t, float, double, string, etc.)
         * @param query SQL query string with placeholders ($1, $2, etc.)
         * @param args Parameter values
         * @return ResultBlock on success, ErrorContext on failure
         *
         * Example: execute("SELECT * FROM users WHERE id = $1 AND active = $2", 42, true)
         *
         * Uses stack-allocated buffer for small parameter sets (< 2KB), falls back to heap if needed.
         */
        template <gears::IsNullTerminatedString QueryT, typename... Args>
            requires(db::IsFieldValueType<Args> && ...)
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(const QueryT& query, Args&&... args) const {
            // Stack buffer for small parameter sets (avoids heap allocation)
            std::array<std::byte, 2048> stack_buffer{};
            std::pmr::monotonic_buffer_resource pool{stack_buffer.data(), stack_buffer.size()};

            ParamSink sink(&pool);

            // Push all parameters as FieldValues
            (sink.push(FieldValue{std::forward<Args>(args)}), ...);

            // Get params and execute (pool stays alive during synchronous execute call)
            const auto params = sink.native_packet();
            return execute_impl(gears::as_c_str(query), params.get());
        }

        /**
         * @brief Execute a query with tuple parameters (convenience overload)
         * @tparam Args Types convertible to FieldValue
         * @param query SQL query string with placeholders ($1, $2, etc.)
         * @param args Parameter values packed in a tuple
         * @return ResultBlock on success, ErrorContext on failure
         *
         * Example: execute("SELECT * FROM users WHERE id = $1 AND active = $2", std::tuple{42, true})
         */
        template <gears::IsNullTerminatedString QueryT, typename... Args>
            requires(db::IsFieldValueType<Args> && ...)
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(const QueryT& query,
                                                                        const std::tuple<Args...>& args) const {
            return std::apply([&](const Args&... a) { return execute(query, a...); }, args);
        }

        /**
         * @brief Execute a compiled static query
         * @tparam Params Parameter types stored in the compiled query
         * @param query CompiledStaticQuery object (from compile_static())
         * @return ResultBlock on success, ErrorContext on failure
         *
         * Example:
         *   constexpr auto compiler = QueryCompiler<PostgresDialect>{};
         *   auto q = compiler.compile_static(select(name).from("users").where(age > 18));
         *   auto result = executor.execute(q);
         */
        template <typename SqlStringT, typename... Params>
            requires(db::IsFieldValueType<Params> && ...)
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext>
        execute(const CompiledStaticQuery<SqlStringT, Params...>& query) const {
            return execute(query.c_sql(), query.params());
        }

        /**
         * @brief Execute a compiled query with parameters
         * @param query Compiled query object
         * @return ResultBlock on success, ErrorContext on failure
         */
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(const CompiledDynamicQuery& query) const;

        [[nodiscard]] PGconn* native_handle() const noexcept {
            return conn_;
        }

    private:
        SCROLL_COMPONENT_PREFIX("SyncExecutor");

        PGconn* conn_ = nullptr;
        std::weak_ptr<ConnectionHolder> holder_;

        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute_impl(const char* query,
                                                                             const Params* params) const;
    };

    static_assert(IsExecutor<SyncExecutor>);

}  // namespace demiplane::db::postgres
