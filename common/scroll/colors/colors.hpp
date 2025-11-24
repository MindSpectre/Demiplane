#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace demiplane::scroll::colors {

    // Reset
    constexpr std::string_view reset = "\033[0m";

    // Regular Colors
    constexpr std::string_view black   = "\033[0;30m";
    constexpr std::string_view red     = "\033[0;31m";
    constexpr std::string_view green   = "\033[0;32m";
    constexpr std::string_view yellow  = "\033[0;33m";
    constexpr std::string_view blue    = "\033[0;34m";
    constexpr std::string_view magenta = "\033[0;35m";  // Purple
    constexpr std::string_view cyan    = "\033[0;36m";
    constexpr std::string_view white   = "\033[0;37m";

    // Bold Colors
    constexpr std::string_view bold_black   = "\033[1;30m";
    constexpr std::string_view bold_red     = "\033[1;31m";
    constexpr std::string_view bold_green   = "\033[1;32m";
    constexpr std::string_view bold_yellow  = "\033[1;33m";
    constexpr std::string_view bold_blue    = "\033[1;34m";
    constexpr std::string_view bold_magenta = "\033[1;35m";
    constexpr std::string_view bold_cyan    = "\033[1;36m";
    constexpr std::string_view bold_white   = "\033[1;37m";

    // Underline Colors
    constexpr std::string_view underline_black   = "\033[4;30m";
    constexpr std::string_view underline_red     = "\033[4;31m";
    constexpr std::string_view underline_green   = "\033[4;32m";
    constexpr std::string_view underline_yellow  = "\033[4;33m";
    constexpr std::string_view underline_blue    = "\033[4;34m";
    constexpr std::string_view underline_magenta = "\033[4;35m";
    constexpr std::string_view underline_cyan    = "\033[4;36m";
    constexpr std::string_view underline_white   = "\033[4;37m";

    // Background Colors
    constexpr std::string_view background_black   = "\033[40m";
    constexpr std::string_view background_red     = "\033[41m";
    constexpr std::string_view background_green   = "\033[42m";
    constexpr std::string_view background_yellow  = "\033[43m";
    constexpr std::string_view background_blue    = "\033[44m";
    constexpr std::string_view background_magenta = "\033[45m";
    constexpr std::string_view background_cyan    = "\033[46m";
    constexpr std::string_view background_white   = "\033[47m";

    // High-Intensity Colors
    constexpr std::string_view hi_black   = "\033[0;90m";
    constexpr std::string_view hi_red     = "\033[0;91m";
    constexpr std::string_view hi_green   = "\033[0;92m";
    constexpr std::string_view hi_yellow  = "\033[0;93m";
    constexpr std::string_view hi_blue    = "\033[0;94m";
    constexpr std::string_view hi_magenta = "\033[0;95m";
    constexpr std::string_view hi_cyan    = "\033[0;96m";
    constexpr std::string_view hi_white   = "\033[0;97m";

    // Bold High-Intensity Colors
    constexpr std::string_view bold_hi_black   = "\033[1;90m";
    constexpr std::string_view bold_hi_red     = "\033[1;91m";
    constexpr std::string_view bold_hi_green   = "\033[1;92m";
    constexpr std::string_view bold_hi_yellow  = "\033[1;93m";
    constexpr std::string_view bold_hi_blue    = "\033[1;94m";
    constexpr std::string_view bold_hi_magenta = "\033[1;95m";
    constexpr std::string_view bold_hi_cyan    = "\033[1;96m";
    constexpr std::string_view bold_hi_white   = "\033[1;97m";

    // High-Intensity Background Colors
    constexpr std::string_view hi_background_black   = "\033[0;100m";
    constexpr std::string_view hi_background_red     = "\033[0;101m";
    constexpr std::string_view hi_background_green   = "\033[0;102m";
    constexpr std::string_view hi_background_yellow  = "\033[0;103m";
    constexpr std::string_view hi_background_blue    = "\033[0;104m";
    constexpr std::string_view hi_background_magenta = "\033[0;105m";
    constexpr std::string_view hi_background_cyan    = "\033[0;106m";
    constexpr std::string_view hi_background_white   = "\033[0;107m";

    // Helper function to wrap text in a given ANSI color code.
    constexpr std::string colorize(const std::string_view code, const std::string_view text) {
        std::string result;
        result.reserve(code.size() + text.size() + reset.size());
        result.append(code).append(text).append(reset);
        return result;
    }

    // Regular colors
    inline std::string make_black(const std::string_view text) {
        return colorize(black, text);
    }
    inline std::string make_red(const std::string_view text) {
        return colorize(red, text);
    }
    inline std::string make_green(const std::string_view text) {
        return colorize(green, text);
    }
    inline std::string make_yellow(const std::string_view text) {
        return colorize(yellow, text);
    }
    inline std::string make_blue(const std::string_view text) {
        return colorize(blue, text);
    }
    inline std::string make_magenta(const std::string_view text) {
        return colorize(magenta, text);
    }
    inline std::string make_cyan(const std::string_view text) {
        return colorize(cyan, text);
    }
    inline std::string make_white(const std::string_view text) {
        return colorize(white, text);
    }

    // Background colors
    inline std::string make_background_black(const std::string_view text) {
        return colorize(background_black, text);
    }
    inline std::string make_background_red(const std::string_view text) {
        return colorize(background_red, text);
    }
    inline std::string make_background_green(const std::string_view text) {
        return colorize(background_green, text);
    }
    inline std::string make_background_yellow(const std::string_view text) {
        return colorize(background_yellow, text);
    }
    inline std::string make_background_blue(const std::string_view text) {
        return colorize(background_blue, text);
    }
    inline std::string make_background_magenta(const std::string_view text) {
        return colorize(background_magenta, text);
    }
    inline std::string make_background_cyan(const std::string_view text) {
        return colorize(background_cyan, text);
    }
    inline std::string make_background_white(const std::string_view text) {
        return colorize(background_white, text);
    }

    // Bold colors
    inline std::string make_bold_black(const std::string_view text) {
        return colorize(bold_black, text);
    }
    inline std::string make_bold_red(const std::string_view text) {
        return colorize(bold_red, text);
    }
    inline std::string make_bold_green(const std::string_view text) {
        return colorize(bold_green, text);
    }
    inline std::string make_bold_yellow(const std::string_view text) {
        return colorize(bold_yellow, text);
    }
    inline std::string make_bold_blue(const std::string_view text) {
        return colorize(bold_blue, text);
    }
    inline std::string make_bold_magenta(const std::string_view text) {
        return colorize(bold_magenta, text);
    }
    inline std::string make_bold_cyan(const std::string_view text) {
        return colorize(bold_cyan, text);
    }
    inline std::string make_bold_white(const std::string_view text) {
        return colorize(bold_white, text);
    }

    // Underline colors
    inline std::string make_underline_black(const std::string_view text) {
        return colorize(underline_black, text);
    }
    inline std::string make_underline_red(const std::string_view text) {
        return colorize(underline_red, text);
    }
    inline std::string make_underline_green(const std::string_view text) {
        return colorize(underline_green, text);
    }
    inline std::string make_underline_yellow(const std::string_view text) {
        return colorize(underline_yellow, text);
    }
    inline std::string make_underline_blue(const std::string_view text) {
        return colorize(underline_blue, text);
    }
    inline std::string make_underline_magenta(const std::string_view text) {
        return colorize(underline_magenta, text);
    }
    inline std::string make_underline_cyan(const std::string_view text) {
        return colorize(underline_cyan, text);
    }
    inline std::string make_underline_white(const std::string_view text) {
        return colorize(underline_white, text);
    }

    // High-intensity colors
    inline std::string make_hi_black(const std::string_view text) {
        return colorize(hi_black, text);
    }
    inline std::string make_hi_red(const std::string_view text) {
        return colorize(hi_red, text);
    }
    inline std::string make_hi_green(const std::string_view text) {
        return colorize(hi_green, text);
    }
    inline std::string make_hi_yellow(const std::string_view text) {
        return colorize(hi_yellow, text);
    }
    inline std::string make_hi_blue(const std::string_view text) {
        return colorize(hi_blue, text);
    }
    inline std::string make_hi_magenta(const std::string_view text) {
        return colorize(hi_magenta, text);
    }
    inline std::string make_hi_cyan(const std::string_view text) {
        return colorize(hi_cyan, text);
    }
    inline std::string make_hi_white(const std::string_view text) {
        return colorize(hi_white, text);
    }

    // Bold high-intensity colors
    inline std::string make_bold_hi_black(const std::string_view text) {
        return colorize(bold_hi_black, text);
    }
    inline std::string make_bold_hi_red(const std::string_view text) {
        return colorize(bold_hi_red, text);
    }
    inline std::string make_bold_hi_green(const std::string_view text) {
        return colorize(bold_hi_green, text);
    }
    inline std::string make_bold_hi_yellow(const std::string_view text) {
        return colorize(bold_hi_yellow, text);
    }
    inline std::string make_bold_hi_blue(const std::string_view text) {
        return colorize(bold_hi_blue, text);
    }
    inline std::string make_bold_hi_magenta(const std::string_view text) {
        return colorize(bold_hi_magenta, text);
    }
    inline std::string make_bold_hi_cyan(const std::string_view text) {
        return colorize(bold_hi_cyan, text);
    }
    inline std::string make_bold_hi_white(const std::string_view text) {
        return colorize(bold_hi_white, text);
    }

    // High-intensity background colors
    inline std::string make_hi_background_black(const std::string_view text) {
        return colorize(hi_background_black, text);
    }
    inline std::string make_hi_background_red(const std::string_view text) {
        return colorize(hi_background_red, text);
    }
    inline std::string make_hi_background_green(const std::string_view text) {
        return colorize(hi_background_green, text);
    }
    inline std::string make_hi_background_yellow(const std::string_view text) {
        return colorize(hi_background_yellow, text);
    }
    inline std::string make_hi_background_blue(const std::string_view text) {
        return colorize(hi_background_blue, text);
    }
    inline std::string make_hi_background_magenta(const std::string_view text) {
        return colorize(hi_background_magenta, text);
    }
    inline std::string make_hi_background_cyan(const std::string_view text) {
        return colorize(hi_background_cyan, text);
    }
    inline std::string make_hi_background_white(const std::string_view text) {
        return colorize(hi_background_white, text);
    }

}  // namespace demiplane::scroll::colors
