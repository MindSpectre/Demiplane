#pragma once
#include <cstdint>
#include <source_location>
#include <thread>

#include "concepts.hpp"
#include "log_level.hpp"
namespace demiplane::scroll {
    namespace detail {
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

        template <class EntryT>
        constexpr bool wants_source = std::is_base_of_v<MetaSource, EntryT>;

        template <class EntryT>
        constexpr bool wants_thread = std::is_base_of_v<MetaThread, EntryT>;
        template <class EntryT>
        constexpr bool wants_process = std::is_base_of_v<MetaProcess, EntryT>;
    } // namespace detail




    template <typename AnyEntry>
    concept IsEntry = gears::always_true_v<AnyEntry>; // TODO: make reliable (e.g. with concept-guards)
} // namespace demiplane::scroll
