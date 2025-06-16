#pragma once
#include <cstdint>
#include <source_location>
#include <thread>

#include "log_level.hpp"

namespace demiplane::scroll::detail {
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
        std::time_t time_point;
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

    protected:
        LogLevel level_;
        std::string message_;
    };
} // namespace demiplane::scroll::detail
