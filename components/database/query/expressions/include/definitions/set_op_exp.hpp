#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <typename Left, typename Right>
    class SetOpExpr : public Expression<SetOpExpr<Left, Right>>,
                      public QueryOperations<SetOpExpr<Left, Right>, AllowOrderBy, AllowLimit> {
    public:
        constexpr SetOpExpr(Left l, Right r, const SetOperation o)
            : left_(std::move(l)),
              right_(std::move(r)),
              op_(o) {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& left(this Self&& self) {
            return std::forward<Self>(self).left_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& right(this Self&& self) {
            return std::forward<Self>(self).right_;
        }

        [[nodiscard]] constexpr SetOperation op() const {
            return op_;
        }

    private:
        Left left_;
        Right right_;
        SetOperation op_;
    };

    // Set operation functions
    template <typename L, typename R>
    constexpr auto union_query(L left, R right) {
        return SetOpExpr<L, R>{std::move(left), std::move(right), SetOperation::UNION};
    }

    template <typename L, typename R>
    constexpr auto union_all(L left, R right) {
        return SetOpExpr<L, R>{std::move(left), std::move(right), SetOperation::UNION_ALL};
    }

    template <typename L, typename R>
    constexpr auto intersect(L left, R right) {
        return SetOpExpr<L, R>{std::move(left), std::move(right), SetOperation::INTERSECT};
    }

    template <typename L, typename R>
    constexpr auto except(L left, R right) {
        return SetOpExpr<L, R>{std::move(left), std::move(right), SetOperation::EXCEPT};
    }
}  // namespace demiplane::db
