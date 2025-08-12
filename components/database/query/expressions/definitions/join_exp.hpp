#pragma once

#include <algorithm>
#include <utility>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query, IsTableSchema JoinedTable, IsCondition Condition>
    class JoinExpr : public Expression<JoinExpr<Query, JoinedTable, Condition>> {
    public:
        constexpr JoinExpr(Query q,
                           JoinedTable jt,
                           Condition c,
                           JoinType t,
                           std::optional<std::string> alias = std::nullopt)
            : query_(std::move(q)),
              joined_table_(std::move(jt)),
              on_condition_(std::move(c)),
              type_(t),
              joined_alias_(std::move(alias)) {}

        template <typename Cond>
        constexpr auto join(TableSchemaPtr right_table, JoinType join_type = JoinType::INNER) const {
            struct JoinBuilder {
                const JoinExpr& parent;
                TableSchemaPtr right_table;
                JoinType type;
                std::optional<std::string> right_alias;

                JoinBuilder(const JoinExpr& parent, TableSchemaPtr right_table, JoinType type)
                    : parent{parent},
                      right_table{std::move(right_table)},
                      type{type} {}

                JoinBuilder& as(std::optional<std::string> name) {
                    right_alias = std::move(name);
                    return *this;
                }

                auto on(Cond cond) const {
                    return JoinExpr<JoinExpr, TableSchemaPtr, Cond>{
                        parent,
                        right_table,
                        std::move(cond),
                        type,
                        right_alias
                    };
                }
            };

            return JoinBuilder{*this, std::move(right_table), join_type};
        }

        template <IsCondition WhereCondition>
        constexpr auto where(WhereCondition cond) const {
            return WhereExpr<JoinExpr, WhereCondition>{*this, std::move(cond)};
        }

        template <IsOrderBy... GroupColumns>
        constexpr auto group_by(GroupColumns... cols) const {
            return GroupByColumnExpr<JoinExpr, GroupColumns...>{*this, cols...};
        }

        template <IsOrderBy... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<JoinExpr, Orders...>{*this, orders...};
        }

        [[nodiscard]] constexpr auto limit(std::size_t count) const {
            return LimitExpr<JoinExpr>{*this, count, 0};
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] auto&& joined_table(this Self&& self) {
            return std::forward<Self>(self).joined_table_;
        }

        template <typename Self>
        [[nodiscard]] auto&& on_condition(this Self&& self) {
            return std::forward<Self>(self).on_condition_;
        }

        [[nodiscard]] JoinType type() const {
            return type_;
        }

        template <typename Self>
        [[nodiscard]] auto&& joined_alias(this Self&& self) {
            return std::forward<Self>(self).joined_alias_;
        }

    private:
        Query query_;
        JoinedTable joined_table_;
        Condition on_condition_;
        JoinType type_;
        std::optional<std::string> joined_alias_;
    };
}
