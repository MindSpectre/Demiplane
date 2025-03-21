#pragma once
#include <optional>

namespace demiplane::database::query {
    /**
     *
     * @brief Paging class
     * @tparam Derived Type of query
     */
    template <typename Derived>
    class LimitOffsetContext {
    public:
        Derived& limit(std::size_t limit_value) {
            limit_ = limit_value;
            return static_cast<Derived&>(*this);
        }

        Derived& offset(std::size_t offset_value) {
            offset_ = offset_value;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] bool has_limit() const noexcept {
            return limit_.has_value();
        }

        [[nodiscard]] bool has_offset() const noexcept {
            return offset_.has_value();
        }

        [[nodiscard]] std::optional<std::size_t> get_limit() const noexcept {
            return limit_;
        }

        [[nodiscard]] std::optional<std::size_t> get_offset() const noexcept {
            return offset_;
        }

    protected:
        std::optional<std::size_t> limit_;
        std::optional<std::size_t> offset_;
    };
} // namespace demiplane::database::query
