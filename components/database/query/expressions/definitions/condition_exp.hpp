#pragma once

#include <algorithm>
#include <vector>

#include "../basic.hpp"
#include "db_column.hpp"


namespace demiplane::db {
    namespace detail {
        template <typename T>
        constexpr auto make_literal_if_needed(T&& value) {
            if constexpr (requires { value.accept(std::declval<QueryVisitor&>()); }) {
                return std::forward<T>(value);
            } else {
                // Raw value, wrap in Literal
                return Literal<std::decay_t<T>>{std::forward<T>(value)};
            }
        }
    }  // namespace detail

    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr : public Expression<BinaryExpr<Left, Right, Op>> {
        public:
        constexpr BinaryExpr(Left l, Right r)
            : left_(std::move(l)),
              right_(std::move(r)) {
        }

        template <typename Self>
        [[nodiscard]] auto&& left(this Self&& self) {
            return std::forward<Self>(self).left_;
        }

        template <typename Self>
        [[nodiscard]] auto&& right(this Self&& self) {
            return std::forward<Self>(self).right_;
        }

        private:
        Left left_;
        Right right_;
    };

    template <typename Operand, IsOperator Op>
    class UnaryExpr : public Expression<UnaryExpr<Operand, Op>> {
        public:
        constexpr explicit UnaryExpr(Operand op)
            : operand_(std::move(op)) {
        }

        template <typename Self>
        [[nodiscard]] auto&& operand(this Self&& self) {
            return std::forward<Self>(self).operand_;
        }

        private:
        Operand operand_;
    };

    template <typename T>
    constexpr auto operator!(T operand) {
        return UnaryExpr<T, OpNot>{std::move(operand)};
    }

    // Helper functions for special operators
    template <typename T>
    constexpr auto is_null(T operand) {
        return UnaryExpr<T, OpIsNull>{std::move(operand)};
    }

    template <typename T>
    constexpr auto is_not_null(T operand) {
        return UnaryExpr<T, OpIsNotNull>{std::move(operand)};
    }

    // Comparison operators
    template <typename LL, typename RR>
    constexpr auto operator==(LL left, RR right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpEqual>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
    constexpr auto operator!=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpNotEqual>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
    constexpr auto operator<(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpLess>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
    constexpr auto operator<=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpLessEqual>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
    constexpr auto operator>(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpGreater>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
    constexpr auto operator>=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpGreaterEqual>{std::move(lv), std::move(rv)};
    }

    // Logical operators
    template <typename L, typename R>
    constexpr auto operator&&(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpAnd>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
    constexpr auto operator||(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpOr>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
    constexpr auto like(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpLike>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
    constexpr auto not_like(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpNotLike>{std::move(lv), std::move(rv)};
    }

    template <typename T>
    constexpr auto in(const TableColumn<T>& col, std::initializer_list<T> values) {
        return BinaryExpr<TableColumn<T>, std::vector<T>, OpIn>{col, std::vector<T>(values)};
    }

    template <typename T, IsQuery Query>
    constexpr auto in(const TableColumn<T>& col, const Subquery<Query>& sq) {
        return BinaryExpr<TableColumn<T>, Subquery<Query>, OpIn>{col, sq};
    }
}  // namespace demiplane::db
