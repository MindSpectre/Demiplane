#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class LimitExpr : public Expression<LimitExpr<Query>> {
    public:
        template <typename QueryTp>
            requires std::constructible_from<Query, QueryTp>
        constexpr LimitExpr(QueryTp&& query, const std::size_t limit, const std::size_t offset) noexcept
            : query_{std::forward<QueryTp>(query)},
              count_{limit},
              offset_{offset} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

        [[nodiscard]] constexpr std::size_t count() const noexcept {
            return count_;
        }

        [[nodiscard]] constexpr std::size_t offset() const noexcept {
            return offset_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto offset(this Self&& self, const std::size_t new_offset) noexcept {
            return LimitExpr{std::forward<Self>(self).query_, self.count_, new_offset};
        }

    private:
        Query query_;
        std::size_t count_;
        std::size_t offset_;
    };
}  // namespace demiplane::db
