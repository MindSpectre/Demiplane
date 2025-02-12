#pragma once

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Предполагается, что FieldBase определён в "db_field.hpp"
#include "db_factory.hpp"
#include "db_shortcuts.hpp"
namespace demiplane::database::query {

    template <typename Derived>
    class TableContext {
    public:
        TableContext()          = default;
        virtual ~TableContext() = default;


        // Задает имя таблицы; возвращает ссылку на себя для поддержки chain-интерфейса.
        Derived& from(const std::string_view table_name) {
            table_name_ = std::string(table_name);
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] const std::string& table() const noexcept {
            return table_name_;
        }

    protected:
        std::string table_name_;
    };

    // ********************************************************************
    // 3. Структуры для хранения условий и сортировки.
    // Эти структуры позволяют сохранить информацию об условии вместе с указателем на поле.
    // Для полей используется std::shared_ptr<FieldBase> – копирование указателя обходится дешевле,
    // чем клонирование каждого объекта.
    // ********************************************************************

    // Структура для условия WHERE.

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

    // Структура для условия ORDER BY.
    struct OrderClause {
        std::shared_ptr<FieldBase> field;
        bool ascending;

        OrderClause(const std::shared_ptr<FieldBase>& f, const bool asc) : field(f), ascending(asc) {}
    };

    // ********************************************************************
    // 4. Миксин-классы (traits) для расширения функционала запроса.
    // Вместо вектора условий здесь используется std::list, что позволяет гибче управлять памятью.
    // ********************************************************************

    // Миксин для добавления условий WHERE.
    template <typename Derived>
    class WhereContext {
    public:
        /**
         * Добавляет условие WHERE.
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

    // Миксин для добавления ORDER BY.
    template <typename Derived>
    class OrderByContext {
    public:
        /**
         * Добавляет условие сортировки.
         *
         * @param field     Умный указатель на FieldBase.
         * @param ascending Если true, сортировка по возрастанию; иначе – по убыванию.
         */
        Derived& order_by(SharedFieldPtr field, const bool ascending = true) {
            order_by_clauses_.emplace_back(std::move(field), ascending);
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

    // Миксин для задания LIMIT и OFFSET.
    template <typename Derived>
    class LimitOffsetContext {
    public:
        Derived& limit(std::size_t limit_value) {
            limit_ = limit_value;
            return static_cast<Derived&>(*this);
        }

        Derived& offset(std::size_t offset_value) {
            offset_ = offset_value;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] bool has_limit() const noexcept {
            return limit_.has_value();
        }

        [[nodiscard]] bool has_offset() const noexcept {
            return offset_.has_value();
        }

        [[nodiscard]] std::optional<std::size_t> get_limit() const noexcept {
            return limit_;
        }

        [[nodiscard]] std::optional<std::size_t> get_offset() const noexcept {
            return offset_;
        }

    protected:
        std::optional<std::size_t> limit_;
        std::optional<std::size_t> offset_;
    };

    // Миксин для добавления условий с использованием SIMILAR TO.
    template <typename Derived>
    class SimilarityConditionContext {
    public:
        /**
         * Добавляет условие SIMILAR TO.
         *
         * @param pattern Шаблон для сравнения.
         */
        Derived& similar(std::string pattern) {
            // Добавляем условие через WhereContext с оператором "SIMILAR TO".
            pattern_ = std::move(pattern);
            return static_cast<Derived&>(*this);
        }

    protected:
        std::string pattern_;
    };

    // ********************************************************************
    // 5. Специализированные классы запросов
    // ********************************************************************

    // ------------------ SELECT ------------------
    class SelectQuery final : public TableContext<SelectQuery>,
                              public WhereContext<SelectQuery>,
                              public OrderByContext<SelectQuery>,
                              public LimitOffsetContext<SelectQuery>,
                              public SimilarityConditionContext<SelectQuery> {
    public:
        /**
         * Задает поля для выборки.
         *
         * @param fields Список умных указателей на FieldBase.
         */
        SelectQuery& select(const FieldCollection& fields) {
            select_fields_ = fields;
            return *this;
        }
        template <typename... Args>
        SelectQuery& select(Args... args) {
            (push_to_list(std::forward<Args>(args)), ...);
            return *this;
        }
        [[nodiscard]] const FieldCollection& get_select_fields() const noexcept {
            return select_fields_;
        }

    private:
        template <typename T>
        requires std::is_base_of_v<FieldBase, std::decay_t<T>>
        void push_to_list(T&& arg) {
            select_fields_.push_back(std::make_shared<std::decay_t<T>>(std::forward<T>(arg)));
        }
        FieldCollection select_fields_;
    };

    // ------------------ INSERT ------------------
    class InsertQuery final : public TableContext<SelectQuery> {
    public:
        InsertQuery& insert(Records&& fields) {
            records_ = std::move(fields);
            return *this;
        }
        // Enforce move out
        [[nodiscard]] Records&& get_records() noexcept {
            return std::move(records_);
        }
        InsertQuery(const InsertQuery& other)                = delete;
        InsertQuery(InsertQuery&& other) noexcept            = default;
        InsertQuery& operator=(const InsertQuery& other)     = delete;
        InsertQuery& operator=(InsertQuery&& other) noexcept = default;

    private:
        Records records_;
    };

    // ------------------ UPDATE ------------------
    class UpdateQuery final : public TableContext<SelectQuery>,
                              public WhereContext<UpdateQuery>,
                              public LimitOffsetContext<UpdateQuery>,
                              public SimilarityConditionContext<UpdateQuery> {
    public:
        UpdateQuery& set(FieldCollection&& fields) {
            update_fields_ = std::move(fields);
            return *this;
        }

        [[nodiscard]] FieldCollection new_values() noexcept {
            return std::move(update_fields_);
        }

        UpdateQuery(const UpdateQuery& other)                = default;
        UpdateQuery(UpdateQuery&& other) noexcept            = default;
        UpdateQuery& operator=(const UpdateQuery& other)     = default;
        UpdateQuery& operator=(UpdateQuery&& other) noexcept = default;

    private:
        FieldCollection update_fields_;
    };

    // ------------------ DELETE ------------------
    class DeleteQuery final : public TableContext<SelectQuery>,
                              public WhereContext<DeleteQuery>,
                              public LimitOffsetContext<DeleteQuery> {
    public:
    };

    // ------------------ UPSERT ------------------
    class UpsertQuery final : public TableContext<SelectQuery>,
                              public WhereContext<UpsertQuery>,
                              public LimitOffsetContext<UpsertQuery> {
    public:
        UpsertQuery& new_values(Records&& fields) {
            records_ = std::move(fields);
            return *this;
        }


        UpsertQuery& update_when_these_fields_occur(FieldCollection conflicting_fields) {
            conflict_fields_ = std::move(conflicting_fields);
            return *this;
        }
        UpsertQuery& replace_these_fields(FieldCollection update_fields) {
            update_fields_ = std::move(update_fields);
            return *this;
        }
        [[nodiscard]] std::optional<FieldCollection> get_conflict_fields() const noexcept {
            return conflict_fields_;
        }
        [[nodiscard]] std::optional<FieldCollection> get_update_fields() const noexcept {
            return update_fields_;
        }
        // Enforce move out
        [[nodiscard]] Records&& get_records() noexcept {
            return std::move(records_);
        }
        UpsertQuery(const UpsertQuery& other)                = delete;
        UpsertQuery(UpsertQuery&& other) noexcept            = default;
        UpsertQuery& operator=(const UpsertQuery& other)     = delete;
        UpsertQuery& operator=(UpsertQuery&& other) noexcept = default;

    private:
        std::optional<FieldCollection>
            conflict_fields_; // Upsert will happen if some of new Records will have same conflict field value
        std::optional<FieldCollection> update_fields_; // On conflict will update this field
        Records records_;
    };

} // namespace demiplane::database::query
