#pragma once
#include <cstring>
#include <demiplane/chrono>
#include <demiplane/gears>
#include <source_location>
#include <thread>

#include "log_level.hpp"

namespace demiplane::scroll::detail {
    template <class EntryT>
    struct entry_traits;


    struct MetaNone {};  // 0-B

    struct MetaSource {
        std::source_location location;

        MetaSource() = default;

        explicit constexpr MetaSource(const std::source_location& loc) noexcept
            : location{loc} {
        }
    };

    struct ThreadLocalCache {
        uint64_t tid;
        int32_t pid;
        char tid_str[16]{};
        char pid_str[16]{};

        ThreadLocalCache() noexcept {
            tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
            pid = getpid();
            snprintf(tid_str, sizeof(tid_str), "%lu", tid);
            snprintf(pid_str, sizeof(pid_str), "%d", pid);
        }
    };

    inline thread_local ThreadLocalCache tl_cache;

    struct MetaThread {
        uint64_t tid;
        char tid_str[16]{};

        MetaThread() noexcept
            : tid{tl_cache.tid} {
            std::memcpy(tid_str, tl_cache.tid_str, sizeof(tid_str));
        }
    };

    struct MetaProcess {
        int32_t pid;
        char pid_str[16]{};

        MetaProcess() noexcept
            : pid{tl_cache.pid} {
            std::memcpy(pid_str, tl_cache.pid_str, sizeof(pid_str));
        }
    };

    struct MetaTimePoint {
        std::chrono::time_point<std::chrono::system_clock> time_point;

        MetaTimePoint() noexcept
            : time_point{chrono::Clock::now()} {
        }
    };

    template <class... Metas>
    class EntryBase : public Metas... {
    public:
        EntryBase(const LogLevel lvl,
                  const std::string_view msg,
                  Metas... metas)  // perfect-forward meta-packs
            : Metas{std::move(metas)}...,
              level_{lvl},
              message_{msg} {
        }

        [[nodiscard]] LogLevel level() const noexcept {
            return level_;
        }

        [[nodiscard]] std::string_view message() const noexcept {
            return message_;
        }

        virtual ~EntryBase()                                = default;
        EntryBase()                                         = default;
        [[nodiscard]] virtual const std::string& to_string() const = 0;

    protected:
        static std::string& get_tl_buffer() {
            thread_local std::string buffer;
            return buffer;
        }

        LogLevel level_{LogLevel::Debug};
        std::string message_;
        static constexpr std::array<const char*, 6> level_strings = {"TRC", "DBG", "INF", "WRN", "ERR", "FAT"};

        [[nodiscard]] constexpr const char* level_cstr() const noexcept {
            return level_strings[static_cast<std::size_t>(level_)];
        }
    };

    template <typename T>
    concept EntryConcept = requires(const T& entry) {
        { entry.level() } -> std::same_as<LogLevel>;
        { entry.message() } -> std::same_as<std::string_view>;
        { entry.to_string() } -> std::same_as<const std::string&>;
    };
}  // namespace demiplane::scroll::detail
