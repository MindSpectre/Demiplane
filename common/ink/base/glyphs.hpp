#pragma once
#include <string_view>

namespace demiplane::ink::border {

    struct Glyphs {
        std::string_view horizontal;
        std::string_view vertical;
        std::string_view top_left;
        std::string_view top_right;
        std::string_view bottom_left;
        std::string_view bottom_right;
        std::string_view tee_top;
        std::string_view tee_bottom;
        std::string_view tee_left;
        std::string_view tee_right;
        std::string_view cross;
    };

    static constexpr Glyphs ascii{
        .horizontal   = "-",
        .vertical     = "|",
        .top_left     = "+",
        .top_right    = "+",
        .bottom_left  = "+",
        .bottom_right = "+",
        .tee_top      = "+",
        .tee_bottom   = "+",
        .tee_left     = "+",
        .tee_right    = "+",
        .cross        = "+",
    };

    static constexpr Glyphs unicode{
        .horizontal   = "\xE2\x94\x80",  // ─
        .vertical     = "\xE2\x94\x82",  // │
        .top_left     = "\xE2\x94\x8C",  // ┌
        .top_right    = "\xE2\x94\x90",  // ┐
        .bottom_left  = "\xE2\x94\x94",  // └
        .bottom_right = "\xE2\x94\x98",  // ┘
        .tee_top      = "\xE2\x94\xAC",  // ┬
        .tee_bottom   = "\xE2\x94\xB4",  // ┴
        .tee_left     = "\xE2\x94\x9C",  // ├
        .tee_right    = "\xE2\x94\xA4",  // ┤
        .cross        = "\xE2\x94\xBC",  // ┼
    };

}  // namespace demiplane::ink::border
