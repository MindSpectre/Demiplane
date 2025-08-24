#pragma once
#include <cstdint>
#include <thread>

#include <demiplane/gears>
#include "log_level.hpp"

namespace demiplane::scroll::detail {
    template <class EntryT>
    struct entry_traits;


    struct MetaNone {}; // 0-B

    struct MetaSource {
        const char* source_file;
        const char* source_func;
        uint32_t source_line;

        MetaSource() = default;

        MetaSource(const char* f, const char* fn, uint32_t l)
            : source_file(f),
              source_func(fn),
              source_line(l) {}
    };

    struct ThreadLocalCache {
        uint64_t tid;
        int32_t pid;
        char tid_str[16]{};
        char pid_str[16]{};

        ThreadLocalCache() {
            tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
            pid = getpid();
            snprintf(tid_str, sizeof(tid_str), "%lu", tid);
            snprintf(pid_str, sizeof(pid_str), "%d", pid);
        }
    };

    inline thread_local ThreadLocalCache tl_cache;

    struct MetaThread {
        uint64_t tid;
        std::string tid_str;

        MetaThread()
            : tid{tl_cache.tid},
              tid_str{tl_cache.tid_str} {}
    };

    struct MetaProcess {
        int32_t pid;
        std::string pid_str;

        MetaProcess()
            : pid{tl_cache.pid},
              pid_str{tl_cache.pid_str} {}
    };

    struct MetaTimePoint {
        std::chrono::time_point<std::chrono::system_clock> time_point;
    };

    template <class... Metas>
    class EntryBase : public Metas... {
    public:
        EntryBase(const LogLevel lvl,
                  const std::string_view msg,
                  Metas... metas) // perfect-forward meta-packs
            : Metas{metas}...,
              level_{lvl},
              message_{msg} {}

        [[nodiscard]] LogLevel level() const {
            return level_;
        }

        [[nodiscard]] std::string_view message() const {
            return message_;
        }

        virtual ~EntryBase() = default;
        EntryBase()          = default;
        [[nodiscard]] virtual std::string to_string() const = 0;

    protected:
        static std::string& get_tl_buffer() {
            thread_local std::string buffer;
            return buffer;
        }

        LogLevel level_{LogLevel::Debug};
        std::string message_;
        static constexpr std::array<const char*, 6> level_strings = {
            "TRACE",
            "DEBUG",
            "INFO ",
            "WARN ",
            "ERROR",
            "FATAL"
        };

        [[nodiscard]] const char* level_cstr() const {
            return level_strings[static_cast<std::size_t>(level_)];
        }
    };

    template <typename T>
    concept EntryConcept = requires(const T& entry) {
        { entry.level() } -> std::same_as<LogLevel>;
        { entry.message() } -> std::same_as<std::string_view>;
        { entry.to_string() } -> std::same_as<std::string>;
    };
} // namespace demiplane::scroll::detail
