#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <gears_concepts.hpp>

#include "colors.hpp"
#include "glyphs.hpp"
#include "layout.hpp"

namespace demiplane::ink {

    class Box {
    public:
        template <gears::IsStringLike BodyTp>
        constexpr explicit Box(BodyTp&& body) noexcept
            : body_{std::forward<BodyTp>(body)} {
        }

        template <typename Self, gears::IsStringLike StringTp>
        [[nodiscard]] constexpr auto&& title(this Self&& self, StringTp&& title) {
            self.title_ = std::forward<StringTp>(title);
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
        const border::Glyphs* glyphs_ = &border::ascii;  // static link, safe
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

            const auto& [horizontal,
                         vertical,
                         top_left,
                         top_right,
                         bottom_left,
                         bottom_right,
                         tee_top,
                         tee_bottom,
                         tee_left,
                         tee_right,
                         cross] = *glyphs_;

            auto wrap_border = [&](std::string s) -> std::string {
                return border_style_.empty() ? std::move(s) : colors::colorize(border_style_, s);
            };

            auto horizontal_run = [&](const std::size_t count) {
                std::string run;
                for (std::size_t i = 0; i < count; ++i) {
                    run.append(horizontal);
                }
                return run;
            };

            // Top border (optionally embedding title).
            std::string top;
            top.append(top_left);
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
            top.append(top_right);

            std::string out;
            out.append(wrap_border(std::move(top)));
            out.push_back('\n');

            const std::string pad_spaces(padding_, ' ');
            for (const auto line : body_lines) {
                out.append(wrap_border(std::string{vertical}));
                out.append(pad_spaces);
                out.append(line);
                out.append(span - 2 * padding_ - detail::visible_width(line), ' ');
                out.append(pad_spaces);
                out.append(wrap_border(std::string{vertical}));
                out.push_back('\n');
            }

            std::string bottom;
            bottom.append(bottom_left);
            bottom.append(horizontal_run(span));
            bottom.append(bottom_right);
            out.append(wrap_border(std::move(bottom)));

            if (terminate_) {
                out.push_back('\n');
            }
            return out;
        }
    };

    template <gears::IsStringLike BodyTp>
    [[nodiscard]] constexpr Box box(BodyTp&& body) {
        return Box{std::forward<BodyTp>(body)};
    }

}  // namespace demiplane::ink
