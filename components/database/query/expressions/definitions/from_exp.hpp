#pragma once

#include <algorithm>

#include "db_table_schema.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query, IsCondition Condition>
    class WhereExpr;

    template <IsQuery Query, typename... Columns>
    class GroupByExpr;

    template <IsQuery Query, IsOrderByExpression... Orders>
    class OrderByExpr;

    template <IsQuery Query>
    class LimitExpr;

    template <IsQuery Query, typename JoinedTable, IsCondition Condition>
    class JoinExpr;

    template <IsQuery Select>
    class FromExpr : public Expression<FromExpr<Select>> {
    public:
        constexpr FromExpr(Select select_q, TableSchemaPtr table)
            : select_(std::move(select_q)),
              table_(std::move(table)) {}

        [[nodiscard]] const Select& select() const {
            return select_;
        }
        [[nodiscard]] const TableSchemaPtr& table() const {
            return table_;
        }
        [[nodiscard]] const char* alias() const {
            return alias_;
        }
        // Table alias
        constexpr FromExpr& as(const char* name) {
            alias_ = name;
            return *this;
        }

        // WHERE clause
        template <IsCondition Condition>
        constexpr auto where(Condition cond) const {
            return WhereExpr<FromExpr, Condition>{*this, std::move(cond)};
        }

        // Direct GROUP BY (no WHERE)
        template <typename... GroupColumns>
        constexpr auto group_by(GroupColumns... cols) const {
            return GroupByExpr<FromExpr, GroupColumns...>{*this, cols...};
        }

        // Direct ORDER BY (no WHERE)
        template <typename... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<FromExpr, Orders...>{*this, orders...};
        }

        // Direct LIMIT (no WHERE)
        [[nodiscard]] constexpr auto limit(std::size_t count) const {
            return LimitExpr<FromExpr>{*this, count, 0};
        }

        struct JoinBuilder {
            const FromExpr& from;
            TableSchemaPtr right_table;
            JoinType type;
            const char* right_alias = nullptr;

            JoinBuilder(const FromExpr& from, TableSchemaPtr right_table, JoinType type)
                : from{from},
                  right_table{std::move(right_table)},
                  type{type} {}

            JoinBuilder& as(const char* name) {
                right_alias = name;
                return *this;
            }
            template <IsCondition Condition>
            auto on(Condition cond) const {
                return JoinExpr<FromExpr, TableSchemaPtr, Condition>{
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
        const char* alias_{nullptr};
    };
}
