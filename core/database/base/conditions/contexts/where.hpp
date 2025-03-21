#pragma once
#include <list>
#include <memory>

#include "../../field/db_factory.hpp"
namespace demiplane::database::query {
    class WhereClause {
    public:
        enum class Operator { EQUAL, GREATER_THAN, LESS_THAN, GREATER_THAN_OR_EQUAL, LESS_THAN_OR_EQUAL, NOT_EQUAL };

        template <typename FieldType>
        WhereClause(Field<FieldType>&& field, const Operator op, FieldType&& value) {
            value_ = std::forward<FieldType>(value);
            value_->set_name(std::forward<FieldType>(field->get_name()));
            operator_ = op;
        }

        template <typename FieldType>
        WhereClause(std::string name, const Operator op, FieldType value) : operator_{op} {
            value_ = utility_factory::shared_field<FieldType>(std::move(name), value);
        }
        [[nodiscard]] std::string name() const& {
            return value_->get_name();
        }
        [[nodiscard]] std::string op() const& {
            switch (operator_) {
            case Operator::EQUAL:
                return "=";
            case Operator::GREATER_THAN:
                return ">";
            case Operator::LESS_THAN:
                return "<";
            case Operator::GREATER_THAN_OR_EQUAL:
                return ">=";
            case Operator::LESS_THAN_OR_EQUAL:
                return "<=";
            case Operator::NOT_EQUAL:
                return "!=";
            }
            throw std::invalid_argument("Invalid operator");
        }
        [[nodiscard]] std::string value() const& {
            return value_->to_string();
        }

    private:
        Operator operator_ = Operator::EQUAL;
        SharedFieldPtr value_;
    };

    template <typename Derived>
    class WhereContext {
    public:
        /**
         * Add WHERE condition.
         *
         * @param args
         */
        template <typename... Args>
        Derived& where(Args&&... args) {
            where_conditions_.emplace_back(std::forward<Args>(args)...);
            return static_cast<Derived&>(*this);
        }
        Derived& where(const WhereClause& where) {
            where_conditions_.emplace_back(where);
            return static_cast<Derived&>(*this);
        }
        Derived& where(WhereClause&& where) {
            where_conditions_.emplace_back(std::move(where));
            return static_cast<Derived&>(*this);
        }
        [[nodiscard]] bool has_where() const noexcept {
            return !where_conditions_.empty();
        }

        [[nodiscard]] const std::list<WhereClause>& get_where_conditions() const noexcept {
            return where_conditions_;
        }

    protected:
        std::list<WhereClause> where_conditions_;
    };
} // namespace demiplane::database::query
