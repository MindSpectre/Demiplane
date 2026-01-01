#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr : public Expression<BinaryExpr<Left, Right, Op>> {
    public:
        template <typename LeftTp, typename RightTp>
            requires std::constructible_from<Left, LeftTp> && std::constructible_from<Right, RightTp>
        constexpr BinaryExpr(LeftTp&& l, RightTp&& r) noexcept
            : left_(std::forward<LeftTp>(l)),
              right_(std::forward<RightTp>(r)) {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& left(this Self&& self) noexcept {
            return std::forward<Self>(self).left_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& right(this Self&& self) noexcept {
            return std::forward<Self>(self).right_;
        }

    private:
        Left left_;
        Right right_;
    };

    template <typename Operand, IsOperator Op>
    class UnaryExpr : public Expression<UnaryExpr<Operand, Op>> {
    public:
        template <typename OperandTp>
            requires std::constructible_from<Operand, OperandTp>
        constexpr explicit UnaryExpr(OperandTp&& op) noexcept
            : operand_(std::forward<OperandTp>(op)) {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& operand(this Self&& self) noexcept {
            return std::forward<Self>(self).operand_;
        }

    private:
        Operand operand_;
    };

    template <IsDbOperand T>
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

    // Comparison operators - constrained to require at least one database operand
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator==(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpEqual>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator!=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpNotEqual>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator<(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpLess>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator<=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpLessEqual>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator>(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpGreater>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator>=(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpGreaterEqual>{std::move(lv), std::move(rv)};
    }

    // Logical operators - constrained to require at least one database operand
    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
    constexpr auto operator&&(L left, R right) {
        auto lv = detail::make_literal_if_needed(std::move(left));
        auto rv = detail::make_literal_if_needed(std::move(right));
        return BinaryExpr<decltype(lv), decltype(rv), OpAnd>{std::move(lv), std::move(rv)};
    }

    template <typename L, typename R>
        requires(IsDbOperand<L> || IsDbOperand<R>)
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

    // IN with subquery - for variadic IN with individual values, use in() from in_list_exp.hpp
    template <typename T, IsQuery Query>
    constexpr auto in(const TableColumn<T>& col, const Subquery<Query>& sq) {
        return BinaryExpr<TableColumn<T>, Subquery<Query>, OpIn>{col, sq};
    }
}  // namespace demiplane::db
