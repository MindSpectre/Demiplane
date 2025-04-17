#pragma once

#include <thread>

#include "db_pool_manager.hpp"

namespace demiplane::database::pool {
    class DbPoolObserver final {
    public:
        void add(DbPoolManager &pool_manager) {
            managers_.emplace_back(pool_manager);
        }
        void link(std::shared_ptr<DbInterfacePool> shared_pool) {
            shared_pool_ = std::move(shared_pool);
        }
        // in dedicated thread
        void run() {
            std::jthread thread = std::jthread([&]() {observe();});
        }
    private:
        void observe() const {
            // while (true) {
            //     for (auto &pool_manager : managers_) {
            //         if (pool_manager.get().is_under_pressure()) {
            //             pool_manager.get().recall_from_shared();
            //         }
            //         else {
            //             pool_manager.get().lend_idle_to_shared();
            //         }
            //     }
            //     std::this_thread::sleep_for(sleep_time_);
            // }
        }
        constexpr static std::chrono::milliseconds sleep_time_{10000};
        std::vector<std::reference_wrapper<DbPoolManager>> managers_;
        std::shared_ptr<DbInterfacePool> shared_pool_;
    };
}