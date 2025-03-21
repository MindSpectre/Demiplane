#pragma once
#include <list>

#include "../../field/db_factory.hpp"
#include "../../field/db_field.hpp"
namespace demiplane::database::query {
    struct OrderClause {
        std::shared_ptr<FieldBase> field;
        bool ascending;

        OrderClause(const std::shared_ptr<FieldBase>& f, const bool asc) : field(f), ascending(asc) {}
    };
    // Миксин для добавления ORDER BY.
    template <typename Derived>
    class OrderByContext {
    public:
        /**
         * Sort condition
         *
         * @param column     By column
         * @param ascending Если true, сортировка по возрастанию; иначе – по убыванию.
         */
        Derived& order_by(SharedFieldPtr column, const bool ascending = true) {
            order_by_clauses_.emplace_back(std::move(column), ascending);
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] bool has_order_by() const noexcept {
            return !order_by_clauses_.empty();
        }

        [[nodiscard]] const std::list<OrderClause>& get_order_by_clauses() const noexcept {
            return order_by_clauses_;
        }

    protected:
        std::list<OrderClause> order_by_clauses_;
    };
}
