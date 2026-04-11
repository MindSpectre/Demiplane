#pragma once

#include <gears_outcome.hpp>
#include <postgres_errors.hpp>

#include "postgres_async_executor.hpp"
#include "postgres_sync_executor.hpp"

namespace demiplane::db {
    template <typename T>
    concept CapabilityProvider = requires(T provider, boost::asio::any_io_executor exec) {
        { provider.with_sync() } -> std::same_as<gears::Outcome<postgres::SyncExecutor, postgres::ErrorContext>>;
        { provider.with_async(exec) } -> std::same_as<gears::Outcome<postgres::AsyncExecutor, postgres::ErrorContext>>;
    };

}  // namespace demiplane::db
