#pragma once

#include <compiled_query/compiled_dynamic_query.hpp>
#include <compiled_query/compiled_static_query.hpp>
#include <gears_strings.hpp>
#include <postgres_params.hpp>
#include <process_pgresult.hpp>

namespace demiplane::db::postgres {

    /// Duck-typed concept for PostgreSQL query executors (SyncExecutor, AsyncExecutor).
    /// Checks the core execute overloads, validity check, and native handle access.
    template <typename T>
    concept IsExecutor = requires(T& exec,
                                  const char* query,
                                  const Params& params,
                                  const FieldValue& field_value,
                                  const std::tuple<FieldValue>& tuple_args,
                                  const CompiledDynamicQuery& dynamic_query,
                                  const CompiledStaticQuery<gears::InlineString<128>>& static_query) {
        { exec.execute(query) };
        { exec.execute(query, params) };
        { exec.execute(query, field_value) };
        { exec.execute(query, tuple_args) };
        { exec.execute(dynamic_query) };
        { exec.execute(static_query) };
        { exec.valid() } -> std::convertible_to<bool>;
        { exec.native_handle() } -> std::same_as<PGconn*>;
    };

}  // namespace demiplane::db::postgres
