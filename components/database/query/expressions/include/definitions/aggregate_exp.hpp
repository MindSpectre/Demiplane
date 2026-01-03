#pragma once

#include <utility>

#include "../basic.hpp"

namespace demiplane::db {
    // Count expression with optional distinct support
    class CountExpr : public AliasableExpression<CountExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires std::constructible_from<DynamicColumn, DynamicColumnTp> &&
                         (!std::same_as<std::remove_cvref_t<DynamicColumnTp>, AllColumns>)
        constexpr CountExpr(DynamicColumnTp&& col, const bool dist) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(col)},
              distinct_{dist} {
        }

        template <typename AllColumnsTp>
            requires std::constructible_from<AllColumns, AllColumnsTp>
        constexpr CountExpr(AllColumnsTp&& col, const bool dist) noexcept
            : ColumnHolder{std::forward<AllColumnsTp>(col)},
              distinct_{dist} {
        }

        [[nodiscard]] constexpr bool distinct() const noexcept {
            return distinct_;
        }

    private:
        bool distinct_ = false;
    };

    // Simple aggregate expressions
    class SumExpr : public AliasableExpression<SumExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires std::constructible_from<DynamicColumn, DynamicColumnTp>
        constexpr explicit SumExpr(DynamicColumnTp&& column) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(column)} {
        }
    };

    class AvgExpr : public AliasableExpression<AvgExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires std::constructible_from<DynamicColumn, DynamicColumnTp>
        constexpr explicit AvgExpr(DynamicColumnTp&& column) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(column)} {
        }
    };

    class MaxExpr : public AliasableExpression<MaxExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires std::constructible_from<DynamicColumn, DynamicColumnTp>
        constexpr explicit MaxExpr(DynamicColumnTp&& column) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(column)} {
        }
    };

    class MinExpr : public AliasableExpression<MinExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires std::constructible_from<DynamicColumn, DynamicColumnTp>
        constexpr explicit MinExpr(DynamicColumnTp&& column) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(column)} {
        }
    };


    // Aggregate function factories
    template <typename T>
    constexpr CountExpr count(const TableColumn<T>& col) noexcept {
        return CountExpr{col.as_dynamic(), false};
    }


    template <typename T>
    constexpr CountExpr count_distinct(const TableColumn<T>& col) noexcept {
        return CountExpr{col.as_dynamic(), true};
    }

    inline CountExpr count_all() noexcept {
        return CountExpr{AllColumns{""}, false};
    }

    inline CountExpr count_all_distinct() noexcept {
        return CountExpr{AllColumns{""}, true};
    }

    template <typename T>
    constexpr SumExpr sum(const TableColumn<T>& col) noexcept {
        return SumExpr{col.as_dynamic()};
    }

    template <typename T>
    constexpr AvgExpr avg(const TableColumn<T>& col) noexcept {
        return AvgExpr{col.as_dynamic()};
    }

    template <typename T>
    constexpr MaxExpr max(const TableColumn<T>& col) noexcept {
        return MaxExpr{col.as_dynamic()};
    }

    template <typename T>
    constexpr MinExpr min(const TableColumn<T>& col) noexcept {
        return MinExpr{col.as_dynamic()};
    }
}  // namespace demiplane::db
