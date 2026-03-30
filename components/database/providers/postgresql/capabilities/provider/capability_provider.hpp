#pragma once

#include "postgres_async_executor.hpp"
#include "postgres_sync_executor.hpp"

namespace demiplane::db {
    template <typename T>
    concept CapabilityProvider = requires(T provider, boost::asio::any_io_executor exec) {
        { provider.with_sync() } -> std::same_as<postgres::SyncExecutor>;
        { provider.with_async(exec) } -> std::same_as<postgres::AsyncExecutor>;
    };

}  // namespace demiplane::db
