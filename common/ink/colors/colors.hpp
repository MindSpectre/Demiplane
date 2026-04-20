#pragma once

#include <string>
#include <string_view>

#include "codes.hpp"

namespace demiplane::ink::colors {

    [[nodiscard]] constexpr std::string colorize(const std::string_view code, const std::string_view text) {
        std::string result;
        result.reserve(code.size() + text.size() + reset.size());
        result.append(code).append(text).append(reset);
        return result;
    }

    [[nodiscard]] constexpr std::string make_black(const std::string_view text) {
        return colorize(black, text);
    }
    [[nodiscard]] constexpr std::string make_red(const std::string_view text) {
        return colorize(red, text);
    }
    [[nodiscard]] constexpr std::string make_green(const std::string_view text) {
        return colorize(green, text);
    }
    [[nodiscard]] constexpr std::string make_yellow(const std::string_view text) {
        return colorize(yellow, text);
    }
    [[nodiscard]] constexpr std::string make_blue(const std::string_view text) {
        return colorize(blue, text);
    }
    [[nodiscard]] constexpr std::string make_magenta(const std::string_view text) {
        return colorize(magenta, text);
    }
    [[nodiscard]] constexpr std::string make_cyan(const std::string_view text) {
        return colorize(cyan, text);
    }
    [[nodiscard]] constexpr std::string make_white(const std::string_view text) {
        return colorize(white, text);
    }

    [[nodiscard]] constexpr std::string make_background_black(const std::string_view text) {
        return colorize(background_black, text);
    }
    [[nodiscard]] constexpr std::string make_background_red(const std::string_view text) {
        return colorize(background_red, text);
    }
    [[nodiscard]] constexpr std::string make_background_green(const std::string_view text) {
        return colorize(background_green, text);
    }
    [[nodiscard]] constexpr std::string make_background_yellow(const std::string_view text) {
        return colorize(background_yellow, text);
    }
    [[nodiscard]] constexpr std::string make_background_blue(const std::string_view text) {
        return colorize(background_blue, text);
    }
    [[nodiscard]] constexpr std::string make_background_magenta(const std::string_view text) {
        return colorize(background_magenta, text);
    }
    [[nodiscard]] constexpr std::string make_background_cyan(const std::string_view text) {
        return colorize(background_cyan, text);
    }
    [[nodiscard]] constexpr std::string make_background_white(const std::string_view text) {
        return colorize(background_white, text);
    }

    [[nodiscard]] constexpr std::string make_bold_black(const std::string_view text) {
        return colorize(bold_black, text);
    }
    [[nodiscard]] constexpr std::string make_bold_red(const std::string_view text) {
        return colorize(bold_red, text);
    }
    [[nodiscard]] constexpr std::string make_bold_green(const std::string_view text) {
        return colorize(bold_green, text);
    }
    [[nodiscard]] constexpr std::string make_bold_yellow(const std::string_view text) {
        return colorize(bold_yellow, text);
    }
    [[nodiscard]] constexpr std::string make_bold_blue(const std::string_view text) {
        return colorize(bold_blue, text);
    }
    [[nodiscard]] constexpr std::string make_bold_magenta(const std::string_view text) {
        return colorize(bold_magenta, text);
    }
    [[nodiscard]] constexpr std::string make_bold_cyan(const std::string_view text) {
        return colorize(bold_cyan, text);
    }
    [[nodiscard]] constexpr std::string make_bold_white(const std::string_view text) {
        return colorize(bold_white, text);
    }

    [[nodiscard]] constexpr std::string make_underline_black(const std::string_view text) {
        return colorize(underline_black, text);
    }
    [[nodiscard]] constexpr std::string make_underline_red(const std::string_view text) {
        return colorize(underline_red, text);
    }
    [[nodiscard]] constexpr std::string make_underline_green(const std::string_view text) {
        return colorize(underline_green, text);
    }
    [[nodiscard]] constexpr std::string make_underline_yellow(const std::string_view text) {
        return colorize(underline_yellow, text);
    }
    [[nodiscard]] constexpr std::string make_underline_blue(const std::string_view text) {
        return colorize(underline_blue, text);
    }
    [[nodiscard]] constexpr std::string make_underline_magenta(const std::string_view text) {
        return colorize(underline_magenta, text);
    }
    [[nodiscard]] constexpr std::string make_underline_cyan(const std::string_view text) {
        return colorize(underline_cyan, text);
    }
    [[nodiscard]] constexpr std::string make_underline_white(const std::string_view text) {
        return colorize(underline_white, text);
    }

    [[nodiscard]] constexpr std::string make_hi_black(const std::string_view text) {
        return colorize(hi_black, text);
    }
    [[nodiscard]] constexpr std::string make_hi_red(const std::string_view text) {
        return colorize(hi_red, text);
    }
    [[nodiscard]] constexpr std::string make_hi_green(const std::string_view text) {
        return colorize(hi_green, text);
    }
    [[nodiscard]] constexpr std::string make_hi_yellow(const std::string_view text) {
        return colorize(hi_yellow, text);
    }
    [[nodiscard]] constexpr std::string make_hi_blue(const std::string_view text) {
        return colorize(hi_blue, text);
    }
    [[nodiscard]] constexpr std::string make_hi_magenta(const std::string_view text) {
        return colorize(hi_magenta, text);
    }
    [[nodiscard]] constexpr std::string make_hi_cyan(const std::string_view text) {
        return colorize(hi_cyan, text);
    }
    [[nodiscard]] constexpr std::string make_hi_white(const std::string_view text) {
        return colorize(hi_white, text);
    }

    [[nodiscard]] constexpr std::string make_bold_hi_black(const std::string_view text) {
        return colorize(bold_hi_black, text);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_red(const std::string_view text) {
        return colorize(bold_hi_red, text);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_green(const std::string_view text) {
        return colorize(bold_hi_green, text);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_yellow(const std::string_view text) {
        return colorize(bold_hi_yellow, text);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_blue(const std::string_view text) {
        return colorize(bold_hi_blue, text);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_magenta(const std::string_view text) {
        return colorize(bold_hi_magenta, text);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_cyan(const std::string_view text) {
        return colorize(bold_hi_cyan, text);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_white(const std::string_view text) {
        return colorize(bold_hi_white, text);
    }

    [[nodiscard]] constexpr std::string make_hi_background_black(const std::string_view text) {
        return colorize(hi_background_black, text);
    }
    [[nodiscard]] constexpr std::string make_hi_background_red(const std::string_view text) {
        return colorize(hi_background_red, text);
    }
    [[nodiscard]] constexpr std::string make_hi_background_green(const std::string_view text) {
        return colorize(hi_background_green, text);
    }
    [[nodiscard]] constexpr std::string make_hi_background_yellow(const std::string_view text) {
        return colorize(hi_background_yellow, text);
    }
    [[nodiscard]] constexpr std::string make_hi_background_blue(const std::string_view text) {
        return colorize(hi_background_blue, text);
    }
    [[nodiscard]] constexpr std::string make_hi_background_magenta(const std::string_view text) {
        return colorize(hi_background_magenta, text);
    }
    [[nodiscard]] constexpr std::string make_hi_background_cyan(const std::string_view text) {
        return colorize(hi_background_cyan, text);
    }
    [[nodiscard]] constexpr std::string make_hi_background_white(const std::string_view text) {
        return colorize(hi_background_white, text);
    }

}  // namespace demiplane::ink::colors
