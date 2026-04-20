#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "colors.hpp"
#include "separators.hpp"
#include "table.hpp"  // pulls in border::Glyphs + ascii/unicode constants

namespace demiplane::ink {

    class Box {
    public:
        constexpr explicit Box(std::string body) noexcept
            : body_{std::move(body)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& title(this Self&& self, std::string t) {
            self.title_ = std::move(t);
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& border(this Self&& self, const border::Glyphs& g) noexcept {
            self.glyphs_ = &g;
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& border_style(this Self&& self, const std::string_view ansi_prefix) noexcept {
            self.border_style_ = ansi_prefix;
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& padding(this Self&& self, const std::size_t n) noexcept {
            self.padding_ = n;
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& terminate(this Self&& self, const bool on = true) noexcept {
            self.terminate_ = on;
            return std::forward<Self>(self);
        }

        [[nodiscard]] constexpr std::string render() const {
            return render_impl();
        }

    private:
        std::string body_;
        std::string title_{};
        const border::Glyphs* glyphs_ = &border::ascii;
        std::string_view border_style_{};
        std::size_t padding_ = 1;
        bool terminate_      = false;

        [[nodiscard]] std::string render_impl() const {
            const auto body_lines = detail::lines(body_);
            std::size_t inner     = 0;
            for (const auto line : body_lines) {
                inner = std::max(inner, detail::visible_width(line));
            }
            const std::size_t title_vw            = detail::visible_width(title_);
            const std::size_t min_inner_for_title = title_.empty() ? 0 : title_vw + 2 /*spaces*/ + 2 /*min dashes*/;
            const std::size_t span                = std::max(inner + 2 * padding_, min_inner_for_title);

            const auto& g = *glyphs_;

            auto wrap_border = [&](std::string s) -> std::string {
                return border_style_.empty() ? std::move(s) : colors::colorize(border_style_, s);
            };

            auto horizontal_run = [&](const std::size_t count) {
                std::string run;
                for (std::size_t i = 0; i < count; ++i) {
                    run.append(g.horizontal);
                }
                return run;
            };

            // Top border (optionally embedding title).
            std::string top;
            top.append(g.top_left);
            if (title_.empty()) {
                top.append(horizontal_run(span));
            } else {
                const std::size_t title_span = title_vw + 2;  // title + surrounding spaces
                const std::size_t left       = (span - title_span) / 2;
                const std::size_t right      = span - title_span - left;
                top.append(horizontal_run(left));
                top.push_back(' ');
                top.append(title_);
                top.push_back(' ');
                top.append(horizontal_run(right));
            }
            top.append(g.top_right);

            std::string out;
            out.append(wrap_border(std::move(top)));
            out.push_back('\n');

            const std::string pad_spaces(padding_, ' ');
            for (const auto line : body_lines) {
                out.append(wrap_border(std::string{g.vertical}));
                out.append(pad_spaces);
                out.append(line);
                out.append(span - 2 * padding_ - detail::visible_width(line), ' ');
                out.append(pad_spaces);
                out.append(wrap_border(std::string{g.vertical}));
                out.push_back('\n');
            }

            std::string bottom;
            bottom.append(g.bottom_left);
            bottom.append(horizontal_run(span));
            bottom.append(g.bottom_right);
            out.append(wrap_border(std::move(bottom)));

            if (terminate_) {
                out.push_back('\n');
            }
            return out;
        }
    };

    [[nodiscard]] constexpr Box box(std::string body) {
        return Box{std::move(body)};
    }

}  // namespace demiplane::ink
