#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace demiplane::scroll {

    /// Transparent hash so `unordered_set<std::string, PrefixHash, std::equal_to<>>`
    /// supports `contains(string_view)` without constructing a temporary std::string.
    struct PrefixHash {
        using is_transparent = void;

        std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        std::size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    enum class PrefixFilterMode : std::uint8_t { AllowAll, Allowlist, Denylist };

    /**
     * @brief Per-sink prefix allow / deny filter.
     *
     * Mutually exclusive modes. Empty prefix is accepted by default in all modes;
     * call @ref block_empty_prefix to reject it.
     */
    class PrefixFilter {
    public:
        using Set = std::unordered_set<std::string, PrefixHash, std::equal_to<>>;

        constexpr PrefixFilter() = default;  // AllowAll

        [[nodiscard]] constexpr static PrefixFilter allow(Set set) {
            return PrefixFilter{PrefixFilterMode::Allowlist, std::move(set)};
        }
        [[nodiscard]] constexpr static PrefixFilter deny(Set set) {
            return PrefixFilter{PrefixFilterMode::Denylist, std::move(set)};
        }

        constexpr PrefixFilter& block_empty_prefix() noexcept {
            block_empty_ = true;
            return *this;
        }

        [[nodiscard]] constexpr bool accepts(const std::string_view prefix) const noexcept {
            if (prefix.empty()) {
                return !block_empty_;
            }
            switch (mode_) {
                case PrefixFilterMode::AllowAll:
                    return true;
                case PrefixFilterMode::Allowlist:
                    return set_.contains(prefix);
                case PrefixFilterMode::Denylist:
                    return !set_.contains(prefix);
            }
            std::unreachable();
        }

        [[nodiscard]] constexpr PrefixFilterMode mode() const noexcept {
            return mode_;
        }
        [[nodiscard]] constexpr bool blocks_empty() const noexcept {
            return block_empty_;
        }

    private:
        constexpr PrefixFilter(const PrefixFilterMode m, Set s)
            : mode_{m},
              set_{std::move(s)} {
        }

        PrefixFilterMode mode_ = PrefixFilterMode::AllowAll;
        bool block_empty_      = false;
        Set set_;
    };

}  // namespace demiplane::scroll
