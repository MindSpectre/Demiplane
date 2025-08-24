#pragma once

#include <demiplane/algorithms>

namespace demiplane::scroll {
    template <detail::EntryConcept EntryType>
    FileLogger<EntryType>::FileLogger(FileLoggerConfig cfg)
        : config_(std::move(cfg)) {
        init();
        running_.store(true, std::memory_order_release); // open first file
        worker_ = std::thread(&FileLogger::writer_loop, this);
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::kill() {
        accepting_.store(false, std::memory_order_release);
        running_.store(false, std::memory_order_release);
        cv_.notify_one();
        if (worker_.joinable()) {
            worker_.join();
        }
        file_stream_.close();
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::graceful_shutdown() {
        accepting_.store(false, std::memory_order_release);

        /* Wait until pending_ becomes 0 */
        std::unique_lock lk(shutdown_mtx_);
        shutdown_cv_.wait(lk, [&] {
            return pending_entries_.load(std::memory_order_acquire) == 0;
        });

        /* Now ask the writer to exit */
        running_.store(false, std::memory_order_release);
        cv_.notify_one();

        if (worker_.joinable()) {
            worker_.join();
        }
        file_stream_.close();
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::log(LogLevel lvl, std::string_view msg, const detail::MetaSource& loc) {
        if (static_cast<int8_t>(lvl) < static_cast<int8_t>(config_.threshold)) {
            return;
        }
        if (!accepting_.load(std::memory_order_relaxed)) {
            return;
        }
        EntryType entry = make_entry<EntryType>(lvl, msg, loc);
        enqueue(std::move(entry));
    }


    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::log(const EntryType& entry) {
        if (static_cast<int8_t>(entry.level()) < static_cast<int8_t>(config_.threshold)) {
            return;
        }
        if (!accepting_.load(std::memory_order_relaxed)) {
            return;
        }
        enqueue(entry);
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::log(EntryType&& entry) {
        if (static_cast<int8_t>(entry.level()) < static_cast<int8_t>(config_.threshold)) {
            return;
        }
        if (!accepting_.load(std::memory_order_relaxed)) {
            return;
        }
        enqueue(std::move(entry));
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::reload() {
        {
            std::unique_lock lk(reload_mtx_);
            reload_done_      = false;
            reload_requested_ = true;
        }
        cv_.notify_one();
        std::unique_lock lk(reload_mtx_);
        reload_cv_.wait(lk, [&] {
            return reload_done_;
        });
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::enqueue(const EntryType& entry) noexcept {
        queue_.enqueue(entry); // copy or move as needed
        pending_entries_.fetch_add(1, std::memory_order_relaxed);
        cv_.notify_one();
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::enqueue(EntryType&& entry) noexcept {
        queue_.enqueue(std::move(entry));
        pending_entries_.fetch_add(1, std::memory_order_relaxed);
        cv_.notify_one();
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::writer_loop() {
        std::vector<EntryType> batch;
        batch.reserve(config_.batch_size);

        while (running_.load(std::memory_order_acquire) || pending_entries_.load(std::memory_order_acquire) > 0) {
            EntryType item;
            while (batch.size() < config_.batch_size && queue_.try_dequeue(item)) {
                batch.emplace_back(std::move(item));
            }

            /* handle reload request before writing */
            if (reload_requested_.load(std::memory_order_acquire)) {
                flush_and_reopen();
                continue;
            }

            if (batch.empty()) {
                std::unique_lock lk(m_);
                // wait 100 ms or when logger will be stopped or reloaded after skip iteration
                cv_.wait_for(lk, std::chrono::milliseconds(40), [&] {
                    return reload_requested_.load(std::memory_order_acquire)
                           || !running_.load(std::memory_order_acquire);
                });
                continue;
            }


            if (config_.sort_entries) {
                static_assert(gears::HasStaticComparator<EntryType>, "This entry type does not support comparison");
                std::sort(batch.begin(), batch.end(), [](const auto& a, const auto& b) {
                    return EntryType::comp(a, b);
                });
            }
            /* write batch */

            // Better approach - single write syscall
            std::string buffer;
            buffer.reserve(batch.size() * 512); // Estimate avg entry size
            for (auto& e : batch) {
                buffer += e.to_string();
            }
            file_stream_ << buffer;
            if (config_.flush_each_batch) {
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
            if (should_rotate()) {
                rotate_log();
            }
        }

        file_stream_.flush();
    }

    template <detail::EntryConcept EntryType>
    bool FileLogger<EntryType>::should_rotate() {
        std::uint64_t sz = 0;
        if (const auto pos = file_stream_.tellp();
            pos >= 0) {
            sz = static_cast<std::uint64_t>(pos);
        }
        else {
            try {
                sz = std::filesystem::file_size(config_.file);
            }
            catch (...) {
                sz = 0;
            }
        }
        return sz > config_.max_file_size;
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::rotate_log() {
        file_stream_.flush();
        file_stream_.close();
        init();
    }

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::flush_and_reopen() {
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

    template <detail::EntryConcept EntryType>
    void FileLogger<EntryType>::init() {
        std::filesystem::path full_path = config_.file;
        if (config_.add_time_to_filename) {
            const std::string stem             = full_path.stem().string();
            const std::string ext              = full_path.extension().string();
            const std::filesystem::path parent = full_path.parent_path();
            const std::string time             = chrono::LocalClock::current_time(config_.time_format_in_file_name);
            full_path                          = parent / (stem + "_" + time + ext);
        }
        if (!full_path.parent_path().empty()) {
            create_directories(full_path.parent_path());
        }

        file_stream_.open(full_path, std::ios::out | std::ios::app);
        if (!file_stream_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + full_path.string());
        }

        file_path_ = full_path;
    }
}
