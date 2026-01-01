#pragma once

#include <utility>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Select, IsTable TableT>
    class FromTableExpr : public AliasableExpression<FromTableExpr<Select, TableT>>,
                          public QueryOperations<FromTableExpr<Select, TableT>,
                                                 AllowGroupBy,
                                                 AllowOrderBy,
                                                 AllowLimit,
                                                 AllowJoin,
                                                 AllowWhere>,
                          public TableHolder<TableT> {
    public:
        template <typename SelectTp, typename TableTp>
            requires std::constructible_from<Select, SelectTp> && std::constructible_from<TableT, TableTp>
        constexpr FromTableExpr(SelectTp&& select_q, TableTp&& table) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(table)},
              select_{std::forward<SelectTp>(select_q)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& select(this Self&& self) noexcept {
            return std::forward<Self>(self).select_;
        }

    private:
        Select select_;
    };

    template <IsQuery Select, IsCteExpr CteQuery>
    class FromCteExpr : public Expression<FromCteExpr<Select, CteQuery>>,
                        public QueryOperations<FromCteExpr<Select, CteQuery>,
                                               AllowGroupBy,
                                               AllowOrderBy,
                                               AllowLimit,
                                               AllowJoin,
                                               AllowWhere> {
    public:
        template <typename SelectTp, typename CteQueryTp>
            requires std::constructible_from<Select, SelectTp> && std::constructible_from<CteQuery, CteQueryTp>
        constexpr FromCteExpr(SelectTp&& select_q, CteQueryTp&& expr) noexcept
            : select_{std::forward<SelectTp>(select_q)},
              query_{std::forward<CteQueryTp>(expr)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& select(this Self&& self) noexcept {
            return std::forward<Self>(self).select_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& cte_query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

    private:
        Select select_;
        CteQuery query_;
    };
}  // namespace demiplane::db
