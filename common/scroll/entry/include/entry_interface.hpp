#pragma once
#include <cinttypes>
#include <cstring>
#include <demiplane/chrono>
#include <demiplane/gears>
#include <source_location>
#include <thread>

#if defined(__linux__)
    #include <sys/syscall.h>
    #include <unistd.h>
#endif

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

    [[nodiscard]] inline uint64_t capture_kernel_tid() noexcept {
#if defined(__linux__)
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 30))
        return static_cast<uint64_t>(::gettid());
    #else
        return static_cast<uint64_t>(::syscall(SYS_gettid));
    #endif
#else
        return std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
    }

    struct ThreadLocalCache {
        uint64_t tid;
        int32_t pid;
        char tid_str[16]{};
        char pid_str[16]{};

        ThreadLocalCache() noexcept {
            tid = capture_kernel_tid();
            pid = getpid();
            snprintf(tid_str, sizeof(tid_str), "%" PRIu64, tid);
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

        virtual ~EntryBase()                             = default;
        EntryBase()                                      = default;
        virtual void format_into(std::string& out) const = 0;

    protected:
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
        { entry.format_into(std::declval<std::string&>()) } -> std::same_as<void>;
    };
}  // namespace demiplane::scroll::detail
