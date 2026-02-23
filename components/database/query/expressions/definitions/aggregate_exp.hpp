#pragma once

#include <utility>

#include "../basic.hpp"

namespace demiplane::db {
    // Count expression with optional distinct support
    class CountExpr : public AliasableExpression<CountExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires IsDynamicColumn<DynamicColumnTp> &&
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
            requires IsDynamicColumn<DynamicColumnTp>
        constexpr explicit SumExpr(DynamicColumnTp&& column) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(column)} {
        }
    };

    class AvgExpr : public AliasableExpression<AvgExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires IsDynamicColumn<DynamicColumnTp>
        constexpr explicit AvgExpr(DynamicColumnTp&& column) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(column)} {
        }
    };

    class MaxExpr : public AliasableExpression<MaxExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires IsDynamicColumn<DynamicColumnTp>
        constexpr explicit MaxExpr(DynamicColumnTp&& column) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(column)} {
        }
    };

    class MinExpr : public AliasableExpression<MinExpr>, public ColumnHolder {
    public:
        template <typename DynamicColumnTp>
            requires IsDynamicColumn<DynamicColumnTp>
        constexpr explicit MinExpr(DynamicColumnTp&& column) noexcept
            : ColumnHolder{std::forward<DynamicColumnTp>(column)} {
        }
    };


    // Aggregate function factories — TableColumn overloads
    template <typename T>
    constexpr CountExpr count(const TableColumn<T>& col) noexcept {
        return CountExpr{col.as_dynamic(), false};
    }

    template <typename T>
    constexpr CountExpr count_distinct(const TableColumn<T>& col) noexcept {
        return CountExpr{col.as_dynamic(), true};
    }

    constexpr CountExpr count_all() noexcept {
        return CountExpr{AllColumns{}, false};
    }

    constexpr CountExpr count_all_distinct() noexcept {
        return CountExpr{AllColumns{}, true};
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

    // Aggregate function factories — DynamicColumn overloads
    template <typename DynamicColumnTp>
        requires IsDynamicColumn<DynamicColumnTp>
    constexpr CountExpr count(DynamicColumnTp&& col) noexcept {
        return CountExpr{std::forward<DynamicColumnTp>(col), false};
    }

    template <typename DynamicColumnTp>
        requires IsDynamicColumn<DynamicColumnTp>
    constexpr CountExpr count_distinct(DynamicColumnTp&& col) noexcept {
        return CountExpr{std::forward<DynamicColumnTp>(col), true};
    }

    template <typename DynamicColumnTp>
        requires IsDynamicColumn<DynamicColumnTp>
    constexpr SumExpr sum(DynamicColumnTp&& col) noexcept {
        return SumExpr{std::forward<DynamicColumnTp>(col)};
    }

    template <typename DynamicColumnTp>
        requires IsDynamicColumn<DynamicColumnTp>
    constexpr AvgExpr avg(DynamicColumnTp&& col) noexcept {
        return AvgExpr{std::forward<DynamicColumnTp>(col)};
    }

    template <typename DynamicColumnTp>
        requires IsDynamicColumn<DynamicColumnTp>
    constexpr MaxExpr max(DynamicColumnTp&& col) noexcept {
        return MaxExpr{std::forward<DynamicColumnTp>(col)};
    }

    template <typename DynamicColumnTp>
        requires IsDynamicColumn<DynamicColumnTp>
    constexpr MinExpr min(DynamicColumnTp&& col) noexcept {
        return MinExpr{std::forward<DynamicColumnTp>(col)};
    }

    // Aggregate function factories — const char* overloads
    constexpr CountExpr count(const char* col) noexcept {
        return CountExpr{DynamicColumn{col}, false};
    }

    constexpr CountExpr count_distinct(const char* col) noexcept {
        return CountExpr{DynamicColumn{col}, true};
    }

    constexpr SumExpr sum(const char* col) noexcept {
        return SumExpr{DynamicColumn{col}};
    }

    constexpr AvgExpr avg(const char* col) noexcept {
        return AvgExpr{DynamicColumn{col}};
    }

    constexpr MaxExpr max(const char* col) noexcept {
        return MaxExpr{DynamicColumn{col}};
    }

    constexpr MinExpr min(const char* col) noexcept {
        return MinExpr{DynamicColumn{col}};
    }
}  // namespace demiplane::db
