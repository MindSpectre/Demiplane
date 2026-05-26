#pragma once

#include <string>
#include <string_view>


namespace demiplane::ink::colors {

    constexpr std::string_view reset = "\033[0m";

    constexpr std::string_view black   = "\033[0;30m";
    constexpr std::string_view red     = "\033[0;31m";
    constexpr std::string_view green   = "\033[0;32m";
    constexpr std::string_view yellow  = "\033[0;33m";
    constexpr std::string_view blue    = "\033[0;34m";
    constexpr std::string_view magenta = "\033[0;35m";
    constexpr std::string_view cyan    = "\033[0;36m";
    constexpr std::string_view white   = "\033[0;37m";

    constexpr std::string_view bold_black   = "\033[1;30m";
    constexpr std::string_view bold_red     = "\033[1;31m";
    constexpr std::string_view bold_green   = "\033[1;32m";
    constexpr std::string_view bold_yellow  = "\033[1;33m";
    constexpr std::string_view bold_blue    = "\033[1;34m";
    constexpr std::string_view bold_magenta = "\033[1;35m";
    constexpr std::string_view bold_cyan    = "\033[1;36m";
    constexpr std::string_view bold_white   = "\033[1;37m";

    constexpr std::string_view underline_black   = "\033[4;30m";
    constexpr std::string_view underline_red     = "\033[4;31m";
    constexpr std::string_view underline_green   = "\033[4;32m";
    constexpr std::string_view underline_yellow  = "\033[4;33m";
    constexpr std::string_view underline_blue    = "\033[4;34m";
    constexpr std::string_view underline_magenta = "\033[4;35m";
    constexpr std::string_view underline_cyan    = "\033[4;36m";
    constexpr std::string_view underline_white   = "\033[4;37m";

    constexpr std::string_view background_black   = "\033[40m";
    constexpr std::string_view background_red     = "\033[41m";
    constexpr std::string_view background_green   = "\033[42m";
    constexpr std::string_view background_yellow  = "\033[43m";
    constexpr std::string_view background_blue    = "\033[44m";
    constexpr std::string_view background_magenta = "\033[45m";
    constexpr std::string_view background_cyan    = "\033[46m";
    constexpr std::string_view background_white   = "\033[47m";

    constexpr std::string_view hi_black   = "\033[0;90m";
    constexpr std::string_view hi_red     = "\033[0;91m";
    constexpr std::string_view hi_green   = "\033[0;92m";
    constexpr std::string_view hi_yellow  = "\033[0;93m";
    constexpr std::string_view hi_blue    = "\033[0;94m";
    constexpr std::string_view hi_magenta = "\033[0;95m";
    constexpr std::string_view hi_cyan    = "\033[0;96m";
    constexpr std::string_view hi_white   = "\033[0;97m";

    constexpr std::string_view bold_hi_black   = "\033[1;90m";
    constexpr std::string_view bold_hi_red     = "\033[1;91m";
    constexpr std::string_view bold_hi_green   = "\033[1;92m";
    constexpr std::string_view bold_hi_yellow  = "\033[1;93m";
    constexpr std::string_view bold_hi_blue    = "\033[1;94m";
    constexpr std::string_view bold_hi_magenta = "\033[1;95m";
    constexpr std::string_view bold_hi_cyan    = "\033[1;96m";
    constexpr std::string_view bold_hi_white   = "\033[1;97m";

    constexpr std::string_view hi_background_black   = "\033[0;100m";
    constexpr std::string_view hi_background_red     = "\033[0;101m";
    constexpr std::string_view hi_background_green   = "\033[0;102m";
    constexpr std::string_view hi_background_yellow  = "\033[0;103m";
    constexpr std::string_view hi_background_blue    = "\033[0;104m";
    constexpr std::string_view hi_background_magenta = "\033[0;105m";
    constexpr std::string_view hi_background_cyan    = "\033[0;106m";
    constexpr std::string_view hi_background_white   = "\033[0;107m";


    [[nodiscard]] constexpr std::string colorize(const std::string_view text, const std::string_view code) {
        std::string result;
        result.reserve(code.size() + text.size() + reset.size());
        result.append(code).append(text).append(reset);
        return result;
    }

    [[nodiscard]] constexpr std::string make_black(const std::string_view text) {
        return colorize(text, black);
    }
    [[nodiscard]] constexpr std::string make_red(const std::string_view text) {
        return colorize(text, red);
    }
    [[nodiscard]] constexpr std::string make_green(const std::string_view text) {
        return colorize(text, green);
    }
    [[nodiscard]] constexpr std::string make_yellow(const std::string_view text) {
        return colorize(text, yellow);
    }
    [[nodiscard]] constexpr std::string make_blue(const std::string_view text) {
        return colorize(text, blue);
    }
    [[nodiscard]] constexpr std::string make_magenta(const std::string_view text) {
        return colorize(text, magenta);
    }
    [[nodiscard]] constexpr std::string make_cyan(const std::string_view text) {
        return colorize(text, cyan);
    }
    [[nodiscard]] constexpr std::string make_white(const std::string_view text) {
        return colorize(text, white);
    }

    [[nodiscard]] constexpr std::string make_background_black(const std::string_view text) {
        return colorize(text, background_black);
    }
    [[nodiscard]] constexpr std::string make_background_red(const std::string_view text) {
        return colorize(text, background_red);
    }
    [[nodiscard]] constexpr std::string make_background_green(const std::string_view text) {
        return colorize(text, background_green);
    }
    [[nodiscard]] constexpr std::string make_background_yellow(const std::string_view text) {
        return colorize(text, background_yellow);
    }
    [[nodiscard]] constexpr std::string make_background_blue(const std::string_view text) {
        return colorize(text, background_blue);
    }
    [[nodiscard]] constexpr std::string make_background_magenta(const std::string_view text) {
        return colorize(text, background_magenta);
    }
    [[nodiscard]] constexpr std::string make_background_cyan(const std::string_view text) {
        return colorize(text, background_cyan);
    }
    [[nodiscard]] constexpr std::string make_background_white(const std::string_view text) {
        return colorize(text, background_white);
    }

    [[nodiscard]] constexpr std::string make_bold_black(const std::string_view text) {
        return colorize(text, bold_black);
    }
    [[nodiscard]] constexpr std::string make_bold_red(const std::string_view text) {
        return colorize(text, bold_red);
    }
    [[nodiscard]] constexpr std::string make_bold_green(const std::string_view text) {
        return colorize(text, bold_green);
    }
    [[nodiscard]] constexpr std::string make_bold_yellow(const std::string_view text) {
        return colorize(text, bold_yellow);
    }
    [[nodiscard]] constexpr std::string make_bold_blue(const std::string_view text) {
        return colorize(text, bold_blue);
    }
    [[nodiscard]] constexpr std::string make_bold_magenta(const std::string_view text) {
        return colorize(text, bold_magenta);
    }
    [[nodiscard]] constexpr std::string make_bold_cyan(const std::string_view text) {
        return colorize(text, bold_cyan);
    }
    [[nodiscard]] constexpr std::string make_bold_white(const std::string_view text) {
        return colorize(text, bold_white);
    }

    [[nodiscard]] constexpr std::string make_underline_black(const std::string_view text) {
        return colorize(text, underline_black);
    }
    [[nodiscard]] constexpr std::string make_underline_red(const std::string_view text) {
        return colorize(text, underline_red);
    }
    [[nodiscard]] constexpr std::string make_underline_green(const std::string_view text) {
        return colorize(text, underline_green);
    }
    [[nodiscard]] constexpr std::string make_underline_yellow(const std::string_view text) {
        return colorize(text, underline_yellow);
    }
    [[nodiscard]] constexpr std::string make_underline_blue(const std::string_view text) {
        return colorize(text, underline_blue);
    }
    [[nodiscard]] constexpr std::string make_underline_magenta(const std::string_view text) {
        return colorize(text, underline_magenta);
    }
    [[nodiscard]] constexpr std::string make_underline_cyan(const std::string_view text) {
        return colorize(text, underline_cyan);
    }
    [[nodiscard]] constexpr std::string make_underline_white(const std::string_view text) {
        return colorize(text, underline_white);
    }

    [[nodiscard]] constexpr std::string make_hi_black(const std::string_view text) {
        return colorize(text, hi_black);
    }
    [[nodiscard]] constexpr std::string make_hi_red(const std::string_view text) {
        return colorize(text, hi_red);
    }
    [[nodiscard]] constexpr std::string make_hi_green(const std::string_view text) {
        return colorize(text, hi_green);
    }
    [[nodiscard]] constexpr std::string make_hi_yellow(const std::string_view text) {
        return colorize(text, hi_yellow);
    }
    [[nodiscard]] constexpr std::string make_hi_blue(const std::string_view text) {
        return colorize(text, hi_blue);
    }
    [[nodiscard]] constexpr std::string make_hi_magenta(const std::string_view text) {
        return colorize(text, hi_magenta);
    }
    [[nodiscard]] constexpr std::string make_hi_cyan(const std::string_view text) {
        return colorize(text, hi_cyan);
    }
    [[nodiscard]] constexpr std::string make_hi_white(const std::string_view text) {
        return colorize(text, hi_white);
    }

    [[nodiscard]] constexpr std::string make_bold_hi_black(const std::string_view text) {
        return colorize(text, bold_hi_black);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_red(const std::string_view text) {
        return colorize(text, bold_hi_red);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_green(const std::string_view text) {
        return colorize(text, bold_hi_green);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_yellow(const std::string_view text) {
        return colorize(text, bold_hi_yellow);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_blue(const std::string_view text) {
        return colorize(text, bold_hi_blue);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_magenta(const std::string_view text) {
        return colorize(text, bold_hi_magenta);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_cyan(const std::string_view text) {
        return colorize(text, bold_hi_cyan);
    }
    [[nodiscard]] constexpr std::string make_bold_hi_white(const std::string_view text) {
        return colorize(text, bold_hi_white);
    }

    [[nodiscard]] constexpr std::string make_hi_background_black(const std::string_view text) {
        return colorize(text, hi_background_black);
    }
    [[nodiscard]] constexpr std::string make_hi_background_red(const std::string_view text) {
        return colorize(text, hi_background_red);
    }
    [[nodiscard]] constexpr std::string make_hi_background_green(const std::string_view text) {
        return colorize(text, hi_background_green);
    }
    [[nodiscard]] constexpr std::string make_hi_background_yellow(const std::string_view text) {
        return colorize(text, hi_background_yellow);
    }
    [[nodiscard]] constexpr std::string make_hi_background_blue(const std::string_view text) {
        return colorize(text, hi_background_blue);
    }
    [[nodiscard]] constexpr std::string make_hi_background_magenta(const std::string_view text) {
        return colorize(text, hi_background_magenta);
    }
    [[nodiscard]] constexpr std::string make_hi_background_cyan(const std::string_view text) {
        return colorize(text, hi_background_cyan);
    }
    [[nodiscard]] constexpr std::string make_hi_background_white(const std::string_view text) {
        return colorize(text, hi_background_white);
    }

}  // namespace demiplane::ink::colors
