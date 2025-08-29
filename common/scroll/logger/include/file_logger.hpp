#pragma once

#include <atomic>
#include <condition_variable>
#include <demiplane/chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include <concurrent_queue.hpp>  // moodycamel
#include <gears_types.hpp>

#include "../logger_interface.hpp"

namespace demiplane::scroll {
    /// @brief Config used in FileLogger
    struct FileLoggerConfig {
        NEXUS_REGISTER(0x6B6D41CE, nexus::Resettable);  // CRC32/ISO-HDLC of demiplane::scroll::FileLoggerConfig

        LogLevel threshold = LogLevel::Debug;
        std::filesystem::path file;

        /// @brief Add time to a file name. Current is iso8601
        bool add_time_to_filename            = true;
        std::string time_format_in_file_name = chrono::clock_formats::iso8601;

        /**
         * @brief Ensures data correctness.
         * Sort entries in batch queries using entry comparator (most default by time).
         * Disabled by default. Extremely slow.
         */
        bool sort_entries     = false;
        /**
         * @brief Ensures data correctness.
         * Enforces flush each batch, even if it is not fill buffer.
         * Disabled by default.
         */
        bool flush_each_batch = false;


        /// @brief The default size is 100 mb.
        std::uint64_t max_file_size = gears::literals::operator""_mb(100);
        /// @brief The default size is 512.
        std::uint32_t batch_size    = 512;
    };

    template <detail::EntryConcept EntryType>
    class FileLogger final : public Logger {
        public:
        NEXUS_REGISTER(0xFCCE2FB1, nexus::Resettable);  // CRC32/ISO-HDLC of demiplane::scroll::FileLogger

        explicit FileLogger(FileLoggerConfig cfg);

        ~FileLogger() override {
            // explicit killing
            kill();
        }

        /// @brief Fast shutdown: no guarantee that queued entries are flushed.
        void kill();

        /// @brief Graceful shutdown: stop accepting, wait until *every* entry is written.
        void graceful_shutdown();

        /*---------------- logging API ----------------------------------------*/

        void log(LogLevel lvl, std::string_view msg, const std::source_location& loc) override;
        void log(const EntryType& entry);

        void log(EntryType&& entry);

        FileLoggerConfig& config() {
            return config_;
        }

        [[nodiscard]] const std::filesystem::path& file_path() const {
            return file_path_;
        }

        /* Reload (unchanged, still works) */
        void reload();

        private:
        /*---------------- enqueue helper (lock-free) --------------------------*/
        void enqueue(const EntryType& entry) noexcept;

        void enqueue(EntryType&& entry) noexcept;

        /*---------------- writer thread --------------------------------------*/
        void writer_loop();

        /*---------------- helpers --------------------------------------------*/
        bool should_rotate();

        void rotate_log();

        void flush_and_reopen();

        /*---------------- init (unchanged) -----------------------------------*/
        void init();

        /*---------------- data members ---------------------------------------*/
        FileLoggerConfig config_;
        std::ofstream file_stream_;
        std::filesystem::path file_path_;
        moodycamel::ConcurrentQueue<EntryType> queue_;

        std::atomic<std::size_t> pending_entries_{0};  // entries enqueued but not yet written
        std::atomic<bool> accepting_{true};            // producers allowed to enqueue?
        std::atomic<bool> running_{true};              // writer loop keeps spinning?

        // entry collectors
        std::thread worker_;
        std::condition_variable cv_;
        std::mutex m_;  // for cv_ sleep

        /* reload handshake */
        std::atomic<bool> reload_requested_{false};
        std::mutex reload_mtx_;
        std::condition_variable reload_cv_;
        bool reload_done_{false};

        /* shutdown handshake */
        std::mutex shutdown_mtx_;
        std::condition_variable shutdown_cv_;
    };
}  // namespace demiplane::scroll

#include "../source/file_logger.inl"
