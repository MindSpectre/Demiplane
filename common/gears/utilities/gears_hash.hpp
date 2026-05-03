#pragma once
#include <string>
#include <string_view>

namespace demiplane::gears {
    /**
     * @brief Transparent hash functor for unordered containers — enables
     *        heterogeneous lookup with `std::string`, `std::string_view`, and
     *        `const char*` without constructing a temporary `std::string`.
     */
    struct StringHash {
        using is_transparent = void;  // Enable heterogeneous lookup

        size_t operator()(const std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }

        size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }

        size_t operator()(const char* s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    /// Transparent equality functor — paired with `StringHash` for
    /// heterogeneous unordered-container lookup.
    struct StringEqual {
        using is_transparent = void;

        bool operator()(const std::string_view lhs, const std::string_view rhs) const noexcept {
            return lhs == rhs;
        }

        bool operator()(const std::string& lhs, const std::string_view rhs) const noexcept {
            return lhs == rhs;
        }

        bool operator()(const std::string_view lhs, const std::string& rhs) const noexcept {
            return lhs == rhs;
        }

        bool operator()(const std::string& lhs, const std::string& rhs) const noexcept {
            return lhs == rhs;
        }
    };

}  // namespace demiplane::gears
