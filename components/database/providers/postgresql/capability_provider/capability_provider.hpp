#pragma once

#include <concepts>

#include <boost/asio/any_io_executor.hpp>
#include <postgres_async_executor.hpp>
#include <postgres_sync_executor.hpp>

namespace demiplane::db::postgres {

    template <typename T>
    concept CapabilityProvider = requires(T provider, boost::asio::any_io_executor exec) {
        { provider.with_sync() } -> std::same_as<SyncExecutor>;
        { provider.with_async(exec) } -> std::same_as<AsyncExecutor>;
    };

    // TODO: Add static_assert verification that Session, Transaction, and AutoTransaction
    //       satisfy CapabilityProvider. Place in a .cpp that includes all relevant headers,
    //       e.g. session.cpp:
    //         static_assert(CapabilityProvider<Session>);
    //         static_assert(CapabilityProvider<Transaction>);
    //         static_assert(CapabilityProvider<AutoTransaction>);

}  // namespace demiplane::db::postgres
