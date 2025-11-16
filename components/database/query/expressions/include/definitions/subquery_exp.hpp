#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class Subquery : public AliasableExpression<Subquery<Query>> {
    public:
        template <typename QueryTp>
            requires std::constructible_from<Query, QueryTp>
        constexpr explicit Subquery(QueryTp&& q) noexcept
            : query_{std::forward<QueryTp>(q)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

    private:
        Query query_;
    };

    template <typename QueryTp>
    constexpr auto subquery(QueryTp&& query) noexcept {
        return Subquery<std::remove_cvref_t<QueryTp>>{std::forward<QueryTp>(query)};
    }
}  // namespace demiplane::db
