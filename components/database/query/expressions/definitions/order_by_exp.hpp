#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    enum class OrderDirection { ASC, DESC };

    template <IsColumnLike ColT>
    class OrderBy : public ColumnHolder<ColT> {
    public:
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit OrderBy(ColumnTp&& col, const OrderDirection dir = OrderDirection::ASC) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(col)},
              direction_{dir} {
        }

        [[nodiscard]] constexpr OrderDirection direction() const noexcept {
            return direction_;
        }

    private:
        OrderDirection direction_;
    };

    // Unified factories — IsColumnLike

    template <IsColumnLike ColT>
    constexpr auto asc(ColT&& col) noexcept {
        return OrderBy<std::remove_cvref_t<ColT>>{std::forward<ColT>(col), OrderDirection::ASC};
    }

    template <IsColumnLike ColT>
    constexpr auto desc(ColT&& col) noexcept {
        return OrderBy<std::remove_cvref_t<ColT>>{std::forward<ColT>(col), OrderDirection::DESC};
    }

    template <IsQuery Query, IsOrderBy... Orders>
    class OrderByExpr : public Expression<OrderByExpr<Query, Orders...>>,
                        public QueryOperations<OrderByExpr<Query, Orders...>, AllowLimit> {
    public:
        template <typename QueryTp, typename... OrdersTp>
            requires std::constructible_from<Query, QueryTp> && (std::constructible_from<Orders, OrdersTp> && ...)
        constexpr explicit OrderByExpr(QueryTp&& q, OrdersTp&&... o) noexcept
            : query_{std::forward<QueryTp>(q)},
              orders_{std::forward<OrdersTp>(o)...} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& orders(this Self&& self) noexcept {
            return std::forward<Self>(self).orders_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.query_), std::forward_like<Self>(self.orders_));
        }

    private:
        Query query_;
        std::tuple<Orders...> orders_;
    };
}  // namespace demiplane::db
