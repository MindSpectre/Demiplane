#pragma once

#include <algorithm>
#include <utility>

#include "db_table_schema.hpp"
#include "group_by_exp.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Select>
    class FromTableExpr : public AliasableExpression<FromTableExpr<Select>> {
    public:
        constexpr FromTableExpr(Select select_q, TableSchemaPtr table)
            : select_(std::move(select_q)),
              table_(std::move(table)) {}

        [[nodiscard]] const Select& select() const {
            return select_;
        }

        [[nodiscard]] const TableSchemaPtr& table() const {
            return table_;
        }

        // WHERE clause
        template <IsCondition Condition>
        constexpr auto where(Condition cond) const {
            return WhereExpr<FromTableExpr, Condition>{*this, std::move(cond)};
        }

        template <IsQuery GroupingCriteria>
        constexpr auto group_by(GroupingCriteria&& query) const {
            return GroupByQueryExpr<FromTableExpr, GroupingCriteria>{*this, std::forward<GroupingCriteria>(query)};
        }

        // Direct GROUP BY (no WHERE)
        template <IsColumn... GroupColumns>
        constexpr auto group_by(GroupColumns... cols) const {
            return GroupByColumnExpr<FromTableExpr, GroupColumns...>{*this, cols...};
        }

        // Direct ORDER BY (no WHERE)
        template <IsOrderBy... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<FromTableExpr, Orders...>{*this, orders...};
        }

        // Direct LIMIT (no WHERE)
        [[nodiscard]] constexpr auto limit(std::size_t count) const {
            return LimitExpr<FromTableExpr>{*this, count, 0};
        }

        struct JoinBuilder {
            const FromTableExpr& from;
            TableSchemaPtr right_table;
            JoinType type;
            std::optional<std::string> right_alias;

            JoinBuilder(const FromTableExpr& from, TableSchemaPtr right_table, JoinType type)
                : from{from},
                  right_table{std::move(right_table)},
                  type{type} {}

            JoinBuilder& as(std::optional<std::string> name) {
                right_alias = std::move(name);
                return *this;
            }

            template <IsCondition Condition>
            auto on(Condition cond) const {
                return JoinExpr<FromTableExpr, TableSchemaPtr, Condition>{
                    from,
                    right_table,
                    std::move(cond),
                    type,
                    right_alias
                };
            }
        };

        // JOIN - returns JoinBuilder

        constexpr auto join(TableSchemaPtr right_table, JoinType type = JoinType::INNER) const {
            // Helper to build join with condition
            return JoinBuilder{*this, std::move(right_table), type};
        }

        // Overload for string table name
        auto join(const std::string& table_name, JoinType type = JoinType::INNER) const {
            auto schema = std::make_shared<TableSchema>(table_name);
            return join(std::move(schema), type);
        }

    private:
        Select select_;
        TableSchemaPtr table_;
    };

    template <IsQuery Select, IsQuery FromAnotherQuery>
    class FromQueryExpr : public Expression<FromQueryExpr<Select, FromAnotherQuery>> {
    public:
        constexpr FromQueryExpr(Select select_q, FromAnotherQuery&& expr)
            : select_(std::move(select_q)),
              query_(std::forward<FromAnotherQuery>(expr)) {}

        [[nodiscard]] const Select& select() const {
            return select_;
        }

        [[nodiscard]] const FromAnotherQuery& query() const {
            return query_;
        }

        // WHERE clause
        template <IsCondition Condition>
        constexpr auto where(Condition cond) const {
            return WhereExpr<FromQueryExpr, Condition>{*this, std::move(cond)};
        }

        // Direct GROUP BY (no WHERE)
        template <IsColumn... GroupColumns>
        constexpr auto group_by(GroupColumns... cols) const {
            return GroupByColumnExpr<FromQueryExpr, GroupColumns...>{*this, cols...};
        }

        template <IsQuery GroupingCriteria>
        constexpr auto group_by(GroupingCriteria&& query) const {
            return GroupByQueryExpr<FromQueryExpr, GroupingCriteria>{*this, std::forward<GroupingCriteria>(query)};
        }

        // Direct ORDER BY (no WHERE)
        template <IsOrderBy... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<FromQueryExpr, Orders...>{*this, orders...};
        }

        // Direct LIMIT (no WHERE)
        [[nodiscard]] constexpr auto limit(std::size_t count) const {
            return LimitExpr<FromQueryExpr>{*this, count, 0};
        }

        struct JoinBuilder {
            const FromQueryExpr& from;
            TableSchemaPtr right_table;
            JoinType type;
            std::optional<std::string> right_alias;

            JoinBuilder(const FromQueryExpr& from, TableSchemaPtr right_table, JoinType type)
                : from{from},
                  right_table{std::move(right_table)},
                  type{type} {}

            JoinBuilder& as(std::optional<std::string> name) {
                right_alias = std::move(name);
                return *this;
            }

            template <IsCondition Condition>
            auto on(Condition cond) const {
                return JoinExpr<FromQueryExpr, TableSchemaPtr, Condition>{
                    from,
                    right_table,
                    std::move(cond),
                    type,
                    right_alias
                };
            }
        };

        // JOIN - returns JoinBuilder

        constexpr auto join(TableSchemaPtr right_table, JoinType type = JoinType::INNER) const {
            // Helper to build join with condition
            return JoinBuilder{*this, std::move(right_table), type};
        }

        // Overload for string table name
        auto join(const std::string& table_name, JoinType type = JoinType::INNER) const {
            auto schema = std::make_shared<TableSchema>(table_name);
            return join(std::move(schema), type);
        }

    private:
        Select select_;
        FromAnotherQuery query_;
    };
}
