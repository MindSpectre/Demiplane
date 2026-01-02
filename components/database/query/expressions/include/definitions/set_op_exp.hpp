#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Left, IsQuery Right>
    class SetOpExpr : public Expression<SetOpExpr<Left, Right>>,
                      public QueryOperations<SetOpExpr<Left, Right>, AllowOrderBy, AllowLimit> {
    public:
        template <typename LeftTp, typename RightTp>
            requires std::constructible_from<Left, LeftTp> && std::constructible_from<Right, RightTp>
        constexpr SetOpExpr(LeftTp&& l, RightTp&& r, const SetOperation o) noexcept
            : left_{std::forward<LeftTp>(l)},
              right_{std::forward<RightTp>(r)},
              op_{o} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& left(this Self&& self) noexcept {
            return std::forward<Self>(self).left_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& right(this Self&& self) noexcept {
            return std::forward<Self>(self).right_;
        }

        [[nodiscard]] constexpr SetOperation op() const noexcept {
            return op_;
        }

    private:
        Left left_;
        Right right_;
        SetOperation op_;
    };

    // Set operation functions
    template <typename LeftTp, typename RightTp>
    constexpr auto union_query(LeftTp&& left, RightTp&& right) {
        return SetOpExpr<std::remove_cvref_t<LeftTp>, std::remove_cvref_t<RightTp>>{
            std::forward<LeftTp>(left), std::forward<RightTp>(right), SetOperation::UNION};
    }

    template <typename LeftTp, typename RightTp>
    constexpr auto union_all(LeftTp&& left, RightTp&& right) {
        return SetOpExpr<std::remove_cvref_t<LeftTp>, std::remove_cvref_t<RightTp>>{
            std::forward<LeftTp>(left), std::forward<RightTp>(right), SetOperation::UNION_ALL};
    }

    template <typename LeftTp, typename RightTp>
    constexpr auto intersect(LeftTp&& left, RightTp&& right) {
        return SetOpExpr<std::remove_cvref_t<LeftTp>, std::remove_cvref_t<RightTp>>{
            std::forward<LeftTp>(left), std::forward<RightTp>(right), SetOperation::INTERSECT};
    }

    template <typename LeftTp, typename RightTp>
    constexpr auto except(LeftTp&& left, RightTp&& right) {
        return SetOpExpr<std::remove_cvref_t<LeftTp>, std::remove_cvref_t<RightTp>>{
            std::forward<LeftTp>(left), std::forward<RightTp>(right), SetOperation::EXCEPT};
    }

    // Set operation binary operators
    // | for UNION (set union without duplicates)
    template <IsQuery L, IsQuery R>
    constexpr auto operator|(L&& left, R&& right) {
        return union_query(std::forward<L>(left), std::forward<R>(right));
    }

    // + for UNION ALL (set union with duplicates)
    template <IsQuery L, IsQuery R>
    constexpr auto operator+(L&& left, R&& right) {
        return union_all(std::forward<L>(left), std::forward<R>(right));
    }

    // & for INTERSECT (set intersection)
    template <IsQuery L, IsQuery R>
    constexpr auto operator&(L&& left, R&& right) {
        return intersect(std::forward<L>(left), std::forward<R>(right));
    }

    // - for EXCEPT (set difference)
    template <IsQuery L, IsQuery R>
    constexpr auto operator-(L&& left, R&& right) {
        return except(std::forward<L>(left), std::forward<R>(right));
    }
}  // namespace demiplane::db
