#pragma once

#include <postgres_executor.hpp>

namespace demiplane::db::postgres {
    /**
     * @brief Synchronous PostgreSQL query executor
     *
     * Provides blocking query execution using libpq's synchronous API.
     * Uses ErrorContext for rich error information including SQLSTATE,
     * error messages, hints, and context.
     */
    class SyncExecutor {
    public:
        /**
         * @brief Construct a sync executor with a PostgreSQL connection
         * @param conn PostgreSQL connection (must be valid and connected)
         */
        explicit SyncExecutor(PGconn* conn)
            : conn_(conn) {
        }

        /**
         * @brief Execute a simple query without parameters
         * @param query SQL query string
         * @return ResultBlock on success, ErrorContext on failure
         */
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(std::string_view query) const;


        /**
         * @brief Execute a query with parameters
         * @param query SQL query string with placeholders ($1, $2, etc.)
         * @param params Parameter values
         * @return ResultBlock on success, ErrorContext on failure
         */
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(std::string_view query,
                                                                        const Params& params) const;

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
        template <typename... Args>
            requires(db::IsFieldValueType<Args> && ...)
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(const std::string_view query,
                                                                        Args&&... args) const {
            // Stack buffer for small parameter sets (avoids heap allocation)
            std::array<std::byte, 2048> stack_buffer{};
            std::pmr::monotonic_buffer_resource pool{stack_buffer.data(), stack_buffer.size()};

            ParamSink sink(&pool);

            // Push all parameters as FieldValues
            (sink.push(db::FieldValue{std::forward<Args>(args)}), ...);

            // Get params and execute (pool stays alive during synchronous execute call)
            const auto params = sink.native_packet();
            return execute(query, *params);
        }

        /**
         * @brief Execute a compiled query with parameters
         * @param query Compiled query object
         * @return ResultBlock on success, ErrorContext on failure
         */
        [[nodiscard]] gears::Outcome<ResultBlock, ErrorContext> execute(const CompiledQuery& query) const;

    private:
        PGconn* conn_;
    };
}  // namespace demiplane::db::postgres
