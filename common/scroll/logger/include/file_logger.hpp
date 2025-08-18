#pragma once

#include <atomic>
#include <condition_variable>
#include <demiplane/chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../logger_interface.hpp"
#include "gears_types.hpp"
#include <concurrent_queue.hpp> // moodycamel

namespace demiplane::scroll {

    struct FileLoggerConfig {
        LogLevel threshold{LogLevel::Debug};
        std::filesystem::path file;
        bool add_time_to_name{true};
        bool safe_mode{false}; // extremely slow
        uint64_t max_file_size = gears::literals::operator""_mb(100);
    };

    template <detail::EntryConcept EntryType>
    class FileLogger final : public Logger {
    public:
        explicit FileLogger(FileLoggerConfig cfg) : config_(std::move(cfg)) {
            init();
            running_.store(true, std::memory_order_release); // open first file
            worker_ = std::thread(&FileLogger::writerLoop, this);
        }

        /*---------------- destructor / kill ----------------------------------*/

        ~FileLogger() override {
            kill();
        }

        /// Fast shutdown: no guarantee that queued entries are flushed.
        void kill() {
            accepting_.store(false, std::memory_order_release);
            running_.store(false, std::memory_order_release);
            cv_.notify_one();
            if (worker_.joinable()) {
                worker_.join();
            }
            file_stream_.close();
        }

        /// Graceful shutdown: stop accepting, wait until *every* entry is written.
        void graceful_shutdown() {
            accepting_.store(false, std::memory_order_release);

            /* Wait until pending_ becomes 0 */
            std::unique_lock lk(shutdown_mtx_);
            shutdown_cv_.wait(lk, [&] { return pending_entries_.load(std::memory_order_acquire) == 0; });

            /* Now ask the writer to exit */
            running_.store(false, std::memory_order_release);
            cv_.notify_one();

            if (worker_.joinable()) {
                worker_.join();
            }
            file_stream_.close();
        }

        /*---------------- logging API ----------------------------------------*/

        void log(LogLevel lvl, std::string_view msg, std::source_location loc) override {
            if (static_cast<int8_t>(lvl) < static_cast<int8_t>(config_.threshold)) {
                return;
            }

            EntryType entry = make_entry<EntryType>(lvl, msg, loc);
            enqueue(std::move(entry));
        }

        FileLoggerConfig& config() {
            return config_;
        }

        /* Reload (unchanged, still works) */
        void reload() {
            {
                std::unique_lock lk(reload_mtx_);
                reload_done_      = false;
                reload_requested_ = true;
            }
            cv_.notify_one();
            std::unique_lock lk(reload_mtx_);
            reload_cv_.wait(lk, [&] { return reload_done_; });
        }

    private:
        /*---------------- enqueue helper (lock-free) --------------------------*/
        void enqueue(const EntryType& entry) noexcept {
            queue_.enqueue(entry); // copy or move as needed
            pending_entries_.fetch_add(1, std::memory_order_relaxed);
            cv_.notify_one();
        }
        void enqueue(EntryType&& entry) noexcept {
            queue_.enqueue(std::move(entry));
            pending_entries_.fetch_add(1, std::memory_order_relaxed);
            cv_.notify_one();
        }

        /*---------------- writer thread --------------------------------------*/
        void writerLoop() {
            std::vector<EntryType> batch;
            batch.reserve(512);

            while (running_.load(std::memory_order_acquire) || pending_entries_.load(std::memory_order_acquire) > 0) {
                EntryType item;
                while (queue_.try_dequeue(item)) {
                    batch.emplace_back(std::move(item));
                }

                /* handle reload request before writing */
                if (reload_requested_.load(std::memory_order_acquire)) {
                    flushAndReopen();
                    continue;
                }

                if (batch.empty()) {
                    std::unique_lock lk(m_);
                    cv_.wait_for(lk, std::chrono::milliseconds(100), [&] {
                        return reload_requested_.load(std::memory_order_acquire)
                            || !running_.load(std::memory_order_acquire);
                    });
                    continue;
                }
                if (config_.safe_mode) {
                    static_assert(gears::HasStaticComparator<EntryType>, "This entry type does not support comparison");
                    std::sort(batch.begin(), batch.end(), [](const auto& a, const auto& b) { return EntryType::comp(a, b);});
                }
                /* write batch */
                for (auto& e : batch) {
                    file_stream_ << e.to_string();
                }
                if (config_.safe_mode) {
                    file_stream_.flush();
                }

                /* update counter & possible shutdown notification */
                auto new_pending = pending_entries_.fetch_sub(batch.size(), std::memory_order_acq_rel) - batch.size();
                if (new_pending == 0) {
                    std::lock_guard lg(shutdown_mtx_);
                    shutdown_cv_.notify_all();
                }

                batch.clear();

                /* rotation check (unchanged) */
                if (shouldRotate()) {
                    rotateLog();
                }
            }

            file_stream_.flush();
        }

        /*---------------- helpers --------------------------------------------*/
        bool shouldRotate() {
            std::uint64_t sz = 0;
            if (auto pos = file_stream_.tellp(); pos >= 0) {
                sz = static_cast<std::uint64_t>(pos);
            } else {
                try {
                    sz = std::filesystem::file_size(config_.file);
                } catch (...) {
                    sz = 0;
                }
            }
            return sz > config_.max_file_size;
        }

        void rotateLog() {
            file_stream_.flush();
            file_stream_.close();
            init();
        }

        void flushAndReopen() {
            file_stream_.flush();
            file_stream_.close();
            init();
            {
                std::lock_guard lg(reload_mtx_);
                reload_done_      = true;
                reload_requested_ = false;
            }
            reload_cv_.notify_one();
        }

        /*---------------- init (unchanged) -----------------------------------*/
        void init() {
            auto full_path = config_.file;
            if (config_.add_time_to_name) {
                const auto stem   = full_path.stem().string();
                const auto ext    = full_path.extension().string();
                const auto parent = full_path.parent_path();
                auto time         = chrono::LocalClock::current_time(chrono::clock_formats::eu_dmy_hms);
                full_path         = parent / (stem + "_" + time + ext);
            }
            if (!full_path.parent_path().empty()) {
                create_directories(full_path.parent_path());
            }

            file_stream_.open(full_path, std::ios::out | std::ios::app);
            if (!file_stream_.is_open()) {
                throw std::runtime_error("Failed to open log file: " + full_path.string());
            }

            config_.file = full_path;
        }

        /*---------------- data members ---------------------------------------*/
        FileLoggerConfig config_;
        std::ofstream file_stream_;
        moodycamel::ConcurrentQueue<EntryType> queue_;

        std::atomic<std::size_t> pending_entries_{0}; // entries enqueued but not yet written
        std::atomic<bool> accepting_{true}; // producers allowed to enqueue?
        std::atomic<bool> running_{true}; // writer loop keeps spinning?

        std::thread worker_;
        std::condition_variable cv_;
        std::mutex m_; // for cv_ sleep

        /* reload handshake */
        std::atomic<bool> reload_requested_{false};
        std::mutex reload_mtx_;
        std::condition_variable reload_cv_;
        bool reload_done_{false};

        /* shutdown handshake */
        std::mutex shutdown_mtx_;
        std::condition_variable shutdown_cv_;
    };

} // namespace demiplane::scroll
