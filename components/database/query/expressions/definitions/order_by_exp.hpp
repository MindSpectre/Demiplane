#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    enum class OrderDirection { ASC, DESC };

    class OrderBy : public ColumnHolder {
    public:
        explicit OrderBy(DynamicColumn col, const OrderDirection dir = OrderDirection::ASC)
            : ColumnHolder{std::move(col)},
              direction_(dir) {
        }


        [[nodiscard]] OrderDirection direction() const {
            return direction_;
        }

    private:
        OrderDirection direction_;
    };

    template <typename T>
    OrderBy asc(const TableColumn<T>& col) {
        return OrderBy{col.as_dynamic(), OrderDirection::ASC};
    }

    template <typename T>
    OrderBy desc(const TableColumn<T>& col) {
        return OrderBy{col.as_dynamic(), OrderDirection::DESC};
    }

    inline OrderBy asc(DynamicColumn col) {
        return OrderBy{std::move(col), OrderDirection::ASC};
    }

    inline OrderBy desc(DynamicColumn col) {
        return OrderBy{std::move(col), OrderDirection::DESC};
    }

    template <IsQuery Query, IsOrderBy... Orders>
    class OrderByExpr : public Expression<OrderByExpr<Query, Orders...>>,
                        public QueryOperations<OrderByExpr<Query, Orders...>, AllowLimit> {
    public:
        constexpr explicit OrderByExpr(Query q, Orders... o)
            : query_(std::move(q)),
              orders_(o...) {
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] auto&& orders(this Self&& self) {
            return std::forward<Self>(self).orders_;
        }

    private:
        Query query_;
        std::tuple<Orders...> orders_;
    };
}  // namespace demiplane::db
