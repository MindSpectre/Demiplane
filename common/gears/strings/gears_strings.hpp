#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace demiplane::gears {
    template <std::size_t N>

    struct FixedString {
        char data[N] = {};

        constexpr explicit(false) FixedString(const char (&str)[N]) noexcept {
            std::ranges::copy_n(str, N, data);
        }

        constexpr explicit operator std::string_view() const noexcept {
            return std::string_view{data, N - 1};
        }

        [[nodiscard]] constexpr std::string_view view() const noexcept {
            return std::string_view{data, N - 1};
        }

        constexpr bool operator==(const FixedString& other) const noexcept {
            return std::string_view{*this} == std::string_view{other};
        }

        template <size_t M>
        constexpr bool operator==(const FixedString<M>&) const noexcept {
            return false;
        }
    };

    template <std::size_t N>
    FixedString(const char (&)[N]) -> FixedString<N>;


    /// Fixed-capacity, non-allocating string for constexpr SQL generation.
    /// Satisfies Appendable (operator+= for string_view and char).
    template <std::size_t Capacity>
    struct InlineString {
        std::size_t size_ = 0;
        char data_[Capacity + 1]{};

        constexpr InlineString() noexcept = default;

        /// @throw std::out_of_range if capacity is exceeded
        constexpr InlineString& operator+=(const std::string_view sv) {
            if (size_ + sv.size() > Capacity) {
                throw std::out_of_range("InlineString: capacity exceeded on string_view append");
            }
            for (std::size_t i = 0; i < sv.size(); ++i)
                data_[size_ + i] = sv[i];
            size_ += sv.size();
            return *this;
        }

        /// @throw std::out_of_range if capacity is exceeded
        constexpr InlineString& operator+=(const char c) {
            if (size_ >= Capacity) {
                throw std::out_of_range("InlineString: capacity exceeded on char append");
            }
            data_[size_++] = c;
            return *this;
        }

        [[nodiscard]] constexpr std::string_view view() const noexcept {
            return {data_, size_};
        }

        [[nodiscard]] explicit constexpr operator std::string_view() const noexcept {
            return view();
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept {
            return size_;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }

        [[nodiscard]] constexpr const char* data() const noexcept {
            return data_;
        }

        [[nodiscard]] constexpr const char* c_str() const noexcept {
            return data_;
        }

        constexpr bool operator==(const std::string_view other) const noexcept {
            return view() == other;
        }
    };
}  // namespace demiplane::gears
