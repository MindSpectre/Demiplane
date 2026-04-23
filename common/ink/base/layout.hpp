#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace demiplane::ink {

    enum class Align : std::uint8_t { Left, Center, Right };

    namespace detail {

        // Count display columns in s, skipping SGR escape sequences
        // (\033[ … m) and UTF-8 continuation bytes (10xxxxxx). Each non-continuation
        // byte counts as one column — accurate for ASCII, ANSI-styled text, and
        // Basic-Multilingual-Plane UTF-8 (em-dash, bullets, smart quotes, etc.).
        // Still a coarse approximation for East-Asian wide characters (should be 2)
        // and combining marks (should be 0); those edge cases remain out of scope.
        [[nodiscard]] constexpr std::size_t visible_width(const std::string_view s) noexcept {
            std::size_t w = 0;
            for (std::size_t i = 0; i < s.size(); ++i) {
                const auto c = static_cast<unsigned char>(s[i]);
                if (c == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
                    i += 2;
                    while (i < s.size() && s[i] != 'm') {
                        ++i;
                    }
                    // loop's ++i will advance past the 'm'; fall through
                    continue;
                }
                if ((c & 0xC0) == 0x80) {
                    // UTF-8 continuation byte — part of a multi-byte code point whose
                    // leading byte already contributed one column.
                    continue;
                }
                ++w;
            }
            return w;
        }

        // Split s on '\n'. Returns at least one element even for empty input.
        [[nodiscard]] constexpr std::vector<std::string_view> lines(const std::string_view s) {
            std::vector<std::string_view> out;
            std::size_t start = 0;
            for (std::size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '\n') {
                    out.emplace_back(s.substr(start, i - start));
                    start = i + 1;
                }
            }
            out.emplace_back(s.substr(start));
            return out;
        }

    }  // namespace detail

    [[nodiscard]] constexpr std::string newline(const std::size_t n = 1) {
        std::string out;
        out.resize(n);
        std::ranges::fill(out, '\n');
        return out;
    }

    [[nodiscard]] constexpr std::string hr(const std::size_t width, const char ch = '-') {
        std::string out;
        out.resize(width);
        std::ranges::fill(out, ch);
        return out;
    }

    [[nodiscard]] constexpr std::string indent(const std::string_view body, const std::size_t spaces) {
        if (spaces == 0 || body.empty()) {
            return std::string{body};
        }
        const std::string pad_str(spaces, ' ');
        std::string out;
        out.reserve(body.size() + spaces * 4);
        out.append(pad_str);
        for (std::size_t i = 0; i < body.size(); ++i) {
            out.push_back(body[i]);
            if (body[i] == '\n' && i + 1 < body.size()) {
                out.append(pad_str);
            }
        }
        return out;
    }

    [[nodiscard]] constexpr std::string
    pad(const std::string_view t, const std::size_t width, const Align a = Align::Left) {
        const std::size_t vw = detail::visible_width(t);
        if (vw >= width) {
            return std::string{t};
        }
        const std::size_t missing = width - vw;
        std::string out;
        out.reserve(t.size() + missing);
        switch (a) {
            case Align::Left:
                out.append(t);
                out.append(missing, ' ');
                break;
            case Align::Right:
                out.append(missing, ' ');
                out.append(t);
                break;
            case Align::Center: {
                const std::size_t left  = missing / 2;
                const std::size_t right = missing - left;
                out.append(left, ' ');
                out.append(t);
                out.append(right, ' ');
                break;
            }
        }
        return out;
    }

}  // namespace demiplane::ink
