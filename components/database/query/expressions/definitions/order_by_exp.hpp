#pragma once

#include "db_column.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    enum class OrderDirection {
        ASC,
        DESC
    };

    template <typename T>
    class OrderBy {
    public:
        explicit OrderBy(Column<T> col, const OrderDirection dir = OrderDirection::ASC)
            : column_(col),
              direction_(dir) {}

        template <typename Self>
        [[nodiscard]] auto&& column(this Self&& self) {
            return std::forward<Self>(self).column_;
        }

        [[nodiscard]] OrderDirection direction() const {
            return direction_;
        }

    private:
        Column<T> column_;
        OrderDirection direction_;
    };

    template <typename T>
    OrderBy<T> asc(const Column<T>& col) {
        return OrderBy<T>{col, OrderDirection::ASC};
    }

    template <typename T>
    OrderBy<T> desc(const Column<T>& col) {
        return OrderBy<T>{col, OrderDirection::DESC};
    }

    template <IsQuery Query, IsOrderBy... Orders>
    class OrderByExpr : public Expression<OrderByExpr<Query, Orders...>> {
    public:
        constexpr explicit OrderByExpr(Query q, Orders... o)
            : query_(std::move(q)),
              orders_(o...) {}

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] auto&& orders(this Self&& self) {
            return std::forward<Self>(self).orders_;
        }

        constexpr auto limit(std::size_t count) const {
            return LimitExpr<OrderByExpr>{*this, count, 0};
        }

        constexpr auto limit(std::size_t count, std::size_t offset) const {
            return LimitExpr<OrderByExpr>{*this, count, offset};
        }

    private:
        Query query_;
        std::tuple<Orders...> orders_;
    };
}
