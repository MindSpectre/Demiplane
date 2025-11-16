#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    enum class OrderDirection { ASC, DESC };

    class OrderBy : public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires std::constructible_from<DynamicColumn, DynamicColumnTp>
        constexpr explicit OrderBy(DynamicColumnTp&& col, const OrderDirection dir = OrderDirection::ASC) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(col)},
              direction_{dir} {
        }

        [[nodiscard]] constexpr OrderDirection direction() const noexcept {
            return direction_;
        }

    private:
        OrderDirection direction_;
    };

    template <typename T>
    constexpr OrderBy asc(const TableColumn<T>& col) noexcept {
        return OrderBy{col.as_dynamic(), OrderDirection::ASC};
    }

    template <typename T>
    constexpr OrderBy desc(const TableColumn<T>& col) noexcept {
        return OrderBy{col.as_dynamic(), OrderDirection::DESC};
    }

    template <typename DynamicColumnTp>
        requires std::constructible_from<DynamicColumn, DynamicColumnTp>
    constexpr OrderBy asc(DynamicColumnTp&& col) noexcept {
        return OrderBy{std::forward<DynamicColumnTp>(col), OrderDirection::ASC};
    }

    template <typename DynamicColumnTp>
        requires std::constructible_from<DynamicColumn, DynamicColumnTp>
    constexpr OrderBy desc(DynamicColumnTp&& col) noexcept {
        return OrderBy{std::forward<DynamicColumnTp>(col), OrderDirection::DESC};
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

    private:
        Query query_;
        std::tuple<Orders...> orders_;
    };
}  // namespace demiplane::db
