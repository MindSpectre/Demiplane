#pragma once

#include <utility>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsColumnLike ColT>
    class CountExpr : public AliasableExpression<CountExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr CountExpr(ColumnTp&& col, const bool dist) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(col)},
              distinct_{dist} {
        }

        [[nodiscard]] constexpr bool distinct() const noexcept {
            return distinct_;
        }

    private:
        bool distinct_ = false;
    };

    template <IsColumnLike ColT>
    class SumExpr : public AliasableExpression<SumExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit SumExpr(ColumnTp&& column) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(column)} {
        }
    };

    template <IsColumnLike ColT>
    class AvgExpr : public AliasableExpression<AvgExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit AvgExpr(ColumnTp&& column) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(column)} {
        }
    };

    template <IsColumnLike ColT>
    class MaxExpr : public AliasableExpression<MaxExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit MaxExpr(ColumnTp&& column) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(column)} {
        }
    };

    template <IsColumnLike ColT>
    class MinExpr : public AliasableExpression<MinExpr<ColT>>, public ColumnHolder<ColT> {
    public:
        template <typename ColumnTp>
            requires std::constructible_from<ColT, ColumnTp>
        constexpr explicit MinExpr(ColumnTp&& column) noexcept
            : ColumnHolder<ColT>{std::forward<ColumnTp>(column)} {
        }
    };

    // Unified factories — IsColumnLike (Column, TypedColumn<T>, AllColumns)

    template <IsColumnLike ColT>
    constexpr auto count(ColT&& col) noexcept {
        return CountExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col), false};
    }

    template <IsColumnLike ColT>
    constexpr auto count_distinct(ColT&& col) noexcept {
        return CountExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col), true};
    }

    constexpr auto count_all() noexcept {
        return CountExpr<AllColumns>{AllColumns{}, false};
    }

    constexpr auto count_all_distinct() noexcept {
        return CountExpr<AllColumns>{AllColumns{}, true};
    }

    template <IsColumnLike ColT>
    constexpr auto sum(ColT&& col) noexcept {
        return SumExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col)};
    }

    template <IsColumnLike ColT>
    constexpr auto avg(ColT&& col) noexcept {
        return AvgExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col)};
    }

    template <IsColumnLike ColT>
    constexpr auto max(ColT&& col) noexcept {
        return MaxExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col)};
    }

    template <IsColumnLike ColT>
    constexpr auto min(ColT&& col) noexcept {
        return MinExpr<std::remove_cvref_t<ColT>>{std::forward<ColT>(col)};
    }

    // Convenience factories — const char* (constructs Column inline)

    constexpr auto count(const char* name) noexcept {
        return CountExpr<Column>{Column{name}, false};
    }

    constexpr auto count_distinct(const char* name) noexcept {
        return CountExpr<Column>{Column{name}, true};
    }

    constexpr auto sum(const char* name) noexcept {
        return SumExpr<Column>{Column{name}};
    }

    constexpr auto avg(const char* name) noexcept {
        return AvgExpr<Column>{Column{name}};
    }

    constexpr auto max(const char* name) noexcept {
        return MaxExpr<Column>{Column{name}};
    }

    constexpr auto min(const char* name) noexcept {
        return MinExpr<Column>{Column{name}};
    }
}  // namespace demiplane::db
