#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <typename Left, typename Right>
    class SetOpExpr : public Expression<SetOpExpr<Left, Right>> {
    public:
        SetOpExpr(Left l, Right r, const SetOperation o)
            : left_(std::move(l)),
              right_(std::move(r)),
              op_(o) {}

        [[nodiscard]] const Left& left() const {
            return left_;
        }

        [[nodiscard]] const Right& right() const {
            return right_;
        }

        [[nodiscard]] SetOperation op() const {
            return op_;
        }

    private:
        Left left_;
        Right right_;
        SetOperation op_;
    };

    // Set operation functions
    template <typename L, typename R>
    auto union_query(L left, R right) {
        return SetOpExpr<L, R>{std::move(left), std::move(right), SetOperation::UNION};
    }

    template <typename L, typename R>
    auto union_all(L left, R right) {
        return SetOpExpr<L, R>{std::move(left), std::move(right), SetOperation::UNION_ALL};
    }

    template <typename L, typename R>
    auto intersect(L left, R right) {
        return SetOpExpr<L, R>{std::move(left), std::move(right), SetOperation::INTERSECT};
    }

    template <typename L, typename R>
    auto except(L left, R right) {
        return SetOpExpr<L, R>{std::move(left), std::move(right), SetOperation::EXCEPT};
    }
}
