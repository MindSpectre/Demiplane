#pragma once

#include "db_column.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    enum class OrderDirection {
        ASC,
        DESC
    };

    // Order by expression
    template <typename T>
    class OrderBy {
    public:
        explicit OrderBy(Column<T> col, const OrderDirection dir = OrderDirection::ASC)
            : column_(col),
              direction_(dir) {}

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] OrderDirection direction() const {
            return direction_;
        }

    private:
        Column<T> column_;
        OrderDirection direction_;
    };

    // Helper functions for ordering
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

        [[nodiscard]] const Query& query() const {
            return query_;
        }

        [[nodiscard]] const std::tuple<Orders...>& orders() const {
            return orders_;
        }

        // LIMIT
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