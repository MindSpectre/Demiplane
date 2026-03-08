#pragma once

#include <cstdint>
#include <string>
#include <typeindex>

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

        constexpr InlineString& operator+=(const std::string_view sv) noexcept {
            for (std::size_t i = 0; i < sv.size(); ++i)
                data_[size_ + i] = sv[i];
            size_ += sv.size();
            return *this;
        }

        constexpr InlineString& operator+=(const char c) noexcept {
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

        constexpr bool operator==(const std::string_view other) const noexcept {
            return view() == other;
        }
    };


    namespace literals {
        template <FixedString Str>
        constexpr auto operator""_fs() {
            return Str;
        }

        // Byte
        constexpr unsigned long long operator""_b(const unsigned long long value) {
            return value;
        }

        // Kilobyte
        constexpr unsigned long long operator""_kb(const unsigned long long value) {
            return value * 1024ULL;
        }

        // Megabyte
        constexpr unsigned long long operator""_mb(const unsigned long long value) {
            return value * 1024ULL * 1024ULL;
        }

        // Gigabyte
        constexpr unsigned long long operator""_gb(const unsigned long long value) {
            return value * 1024ULL * 1024ULL * 1024ULL;
        }

        // Terabyte
        constexpr unsigned long long operator""_tb(const unsigned long long value) {
            return value * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        }
    }  // namespace literals

    template <typename T>
    constexpr const char* get_type_name() {
        if constexpr (std::is_same_v<T, int>)
            return "int";
        else if constexpr (std::is_same_v<T, double>)
            return "double";
        else if constexpr (std::is_same_v<T, float>)
            return "float";
        else if constexpr (std::is_same_v<T, bool>)
            return "bool";
        else if constexpr (std::is_same_v<T, std::string>)
            return "string";
        else if constexpr (std::is_same_v<T, std::int32_t>)
            return "int32";
        else if constexpr (std::is_same_v<T, std::int64_t>)
            return "int64";
        else
            return typeid(T).name();  // fallback to mangled name
    }

    inline const char* get_type_name_from_index(const std::type_index& ti) {
        if (ti == std::type_index(typeid(int)))
            return "int";
        if (ti == std::type_index(typeid(double)))
            return "double";
        if (ti == std::type_index(typeid(float)))
            return "float";
        if (ti == std::type_index(typeid(bool)))
            return "bool";
        if (ti == std::type_index(typeid(std::string)))
            return "string";
        if (ti == std::type_index(typeid(std::int32_t)))
            return "int32";
        if (ti == std::type_index(typeid(std::int64_t)))
            return "int64";
        return ti.name();
        // fallback
    }
}  // namespace demiplane::gears
