#pragma once

#include <concepts>
#include <cstddef>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "colors.hpp"
#include "separators.hpp"

namespace demiplane::ink {

    class Section {
    public:
        constexpr explicit Section(std::string title) noexcept
            : title_{std::move(title)} {
        }

        template <typename Self, typename LabelTp, typename ValueTp>
            requires std::convertible_to<LabelTp, std::string_view> &&
                     std::formattable<std::remove_cvref_t<ValueTp>, char>
        [[nodiscard]] constexpr auto&& row(this Self&& self, LabelTp&& label, ValueTp&& value) {
            self.rows_.emplace_back(std::string{std::string_view{std::forward<LabelTp>(label)}},
                                    std::format("{}", std::forward<ValueTp>(value)));
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& title_style(this Self&& self, const std::string_view ansi_prefix) noexcept {
            self.title_style_ = ansi_prefix;
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& indent_size(this Self&& self, const std::size_t n) noexcept {
            self.indent_ = n;
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& separator(this Self&& self, const std::string_view s) noexcept {
            self.separator_ = s;
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
        std::string title_{};
        std::string_view title_style_{};
        std::size_t indent_         = 2;
        std::string_view separator_ = "  ";  // spaces after the colon; default two
        bool terminate_             = false;
        std::vector<std::pair<std::string, std::string>> rows_{};

        [[nodiscard]] std::string render_impl() const {
            std::string out;

            if (!title_.empty()) {
                if (title_style_.empty()) {
                    out.append(title_);
                } else {
                    out.append(colors::colorize(title_style_, title_));
                }
            }

            std::size_t max_label = 0;
            for (const auto& label : rows_ | std::views::keys) {
                max_label = std::max(max_label, detail::visible_width(label));
            }

            const std::string indent_str(indent_, ' ');
            const std::size_t value_col = indent_ + max_label + 1 + separator_.size();
            const std::string continuation(value_col, ' ');

            for (const auto& [label, value] : rows_) {
                if (!out.empty()) {
                    out.push_back('\n');
                }
                out.append(indent_str);
                out.append(label);
                out.push_back(':');
                const std::size_t label_vw = detail::visible_width(label);
                out.append(max_label - label_vw + separator_.size(), ' ');

                const auto value_lines = detail::lines(value);
                for (std::size_t i = 0; i < value_lines.size(); ++i) {
                    if (i > 0) {
                        out.push_back('\n');
                        out.append(continuation);
                    }
                    out.append(value_lines[i]);
                }
            }

            if (terminate_) {
                out.push_back('\n');
            }
            return out;
        }
    };

    [[nodiscard]] constexpr Section section(std::string title) {
        return Section{std::move(title)};
    }

}  // namespace demiplane::ink
