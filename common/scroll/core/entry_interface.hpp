#pragma once
#include <cstdint>
#include <source_location>
#include <thread>

#include "log_level.hpp"

namespace demiplane::scroll::detail {
    template <class EntryT>
    struct entry_traits;


    struct MetaNone {}; // 0-B
    struct MetaSource { // file / line / func
        std::source_location loc;
    };
    struct MetaThread {
        uint64_t tid;
    };
    struct MetaProcess {
        int32_t pid;
    };
    struct MetaTimePoint {
        std::chrono::time_point<std::chrono::system_clock> time_point;
    };
    // … add MetaTime, MetaSpanCtx, …

    template <class... Metas>
    class EntryBase : public Metas... {
    public:
        EntryBase(const LogLevel lvl, const std::string_view msg,
            Metas... metas) // perfect-forward meta-packs
            : Metas{metas}..., level_{lvl}, message_{msg} {}

        [[nodiscard]] LogLevel level() const {
            return level_;
        }
        [[nodiscard]] std::string_view message() const {
            return message_;
        }
        virtual ~EntryBase() = default;
        [[nodiscard]] virtual std::string to_string() const = 0;
    protected:
        LogLevel level_;
        std::string message_;
    };

    template <typename T>
    concept EntryConcept = requires(const T& entry) {
        { entry.level() } -> std::same_as<LogLevel>;
        { entry.message() } -> std::same_as<std::string_view>;
        { entry.to_string() } -> std::same_as<std::string>;
    };

} // namespace demiplane::scroll::detail
