#include "pool_janitor.hpp"

#include <condition_variable>
#include <mutex>

#include <postgres_errors.hpp>

namespace demiplane::db::postgres {

    PoolJanitor::PoolJanitor(ConnectionPool& pool)
        : pool_{pool},
          thread_{[this](const std::stop_token& token) { run(token); }} {
    }

    PoolJanitor::~PoolJanitor() {
        stop();
    }

    void PoolJanitor::stop() {
        thread_.request_stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void PoolJanitor::run(const std::stop_token& token) const {
        const auto interval = pool_.pool_config().health_check_interval();

        std::condition_variable_any cv;
        std::mutex mtx;

        while (!token.stop_requested()) {
            {
                std::unique_lock lock{mtx};
                cv.wait_for(lock, token, interval, [] { return false; });
            }

            if (token.stop_requested()) {
                break;
            }

            sweep(token);
        }
    }

    void PoolJanitor::sweep(const std::stop_token& token) const {
        auto& slots    = pool_.slots();
        const auto cap = pool_.capacity();

        for (std::size_t i = 0; i < cap; ++i) {
            if (token.stop_requested()) {
                return;
            }

            switch (auto& slot = slots[i]; slot.status.load(std::memory_order_relaxed)) {
                case SlotStatus::DEAD: {
                    // Replace dead connection
                    if (slot.conn) {
                        PQfinish(slot.conn);
                        slot.conn = nullptr;
                    }

                    if (PGconn* new_conn = pool_.create_connection()) {
                        slot.conn = new_conn;
                        slot.status.store(SlotStatus::FREE, std::memory_order_release);
                    }
                    break;
                }

                case SlotStatus::FREE: {
                    // Health-check free connections
                    if (slot.conn && check_connection(slot.conn).is_success()) {
                        break;
                    }

                    // Connection is bad -- CAS to avoid racing with acquire()
                    if (auto expected = SlotStatus::FREE; slot.status.compare_exchange_strong(
                            expected, SlotStatus::DEAD, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        if (slot.conn) {
                            PQfinish(slot.conn);
                            slot.conn = nullptr;
                        }
                        // Try immediate replacement
                        if (PGconn* new_conn = pool_.create_connection()) {
                            slot.conn = new_conn;
                            slot.status.store(SlotStatus::FREE, std::memory_order_release);
                        }
                    }
                    break;
                }

                case SlotStatus::USED:
                case SlotStatus::WAITING:
                case SlotStatus::INACTIVE:
                    break;
            }
        }
    }

}  // namespace demiplane::db::postgres
