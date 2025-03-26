#pragma once

#include "contexts/limit_offset.hpp"
#include "contexts/order_by.hpp"
#include "contexts/returning.hpp"
#include "contexts/similarity.hpp"
#include "contexts/table.hpp"
#include "contexts/where.hpp"
namespace demiplane::database::query {

    template <typename FinalQuery>
    class QueryUtilities {
    public:
        virtual ~QueryUtilities() = default;
        bool use_params           = true;
    };
    // ------------------ SELECT ------------------
    class SelectQuery final : public TableContext<SelectQuery>,
                              public WhereContext<SelectQuery>,
                              public OrderByContext<SelectQuery>,
                              public LimitOffsetContext<SelectQuery>,
                              public SimilarityConditionContext<SelectQuery> {
    public:
        /**
         * Selected columns
         *
         * @param columns Names of columns
         */
        SelectQuery& select(const Columns& columns) {
            selected_columns = columns;
            return *this;
        }
        template <typename... Args>
        SelectQuery& select(Args... args) {
            (push_to_list(std::forward<Args>(args)), ...);
            return *this;
        }
        [[nodiscard]] const Columns& get_select_columns() const noexcept {
            return selected_columns;
        }

    private:
        template <typename T>
            requires std::is_same_v<Column, T>
        void push_to_list(T&& arg) {
            selected_columns.push_back(std::make_shared<std::decay_t<T>>(std::forward<T>(arg)));
        }
        Columns selected_columns;
    };

    // ------------------ INSERT ------------------
    class InsertQuery final : public TableContext<InsertQuery>,
                              public QueryUtilities<InsertQuery>,
                              public Returning<InsertQuery> {
    public:
        InsertQuery& insert(Records&& fields) {
            records_ = std::move(fields);
            return *this;
        }
        // Enforce move out
        [[nodiscard]] Records extract_records() && noexcept {
            return std::move(records_);
        }
        [[nodiscard]] const Records& view_records() const& noexcept {
            return records_;
        }

    private:
        Records records_;
    };

    // ------------------ UPDATE ------------------
    class UpdateQuery final : public TableContext<UpdateQuery>,
                              public WhereContext<UpdateQuery>,
                              public Returning<UpdateQuery> {
    public:
        UpdateQuery& set(FieldCollection&& fields) {
            update_fields_ = std::move(fields);
            return *this;
        }

        [[nodiscard]] FieldCollection extract_new_values() noexcept {
            return std::move(update_fields_);
        }

    private:
        FieldCollection update_fields_;
    };

    // ------------------ DELETE ------------------
    class DeleteQuery final : public TableContext<DeleteQuery>, public WhereContext<DeleteQuery> {};

    // ------------------ UPSERT ------------------
    class UpsertQuery final : public TableContext<UpsertQuery>,
                              public WhereContext<UpsertQuery>,
                              public QueryUtilities<UpsertQuery>,
                              public Returning<UpsertQuery> {
    public:
        UpsertQuery& new_values(Records&& fields) {
            records_ = std::move(fields);
            return *this;
        }


        UpsertQuery& when_conflict_in_these_columns(Columns columns) {
            conflict_columns_ = std::move(columns);
            return *this;
        }
        UpsertQuery& replace_these_columns(Columns columns) {
            update_columns_ = std::move(columns);
            return *this;
        }
        [[nodiscard]] Columns get_conflict_columns() const noexcept {
            return conflict_columns_;
        }
        [[nodiscard]] Columns get_update_columns() const noexcept {
            return update_columns_;
        }
        // Enforce move out
        [[nodiscard]] Records extract_records() && noexcept {
            return std::move(records_);
        }
        [[nodiscard]] const Records& view_records() const& noexcept {
            return records_;
        }

    private:
        Columns conflict_columns_; // Upsert will happen if some of new Records will have same conflict field value
        Columns update_columns_; // On conflict will update this field
        Records records_;
    };

    class CreateQuery final {
    public:
        CreateQuery& columns(Columns columns) {
            columns_ = std::move(columns);
            return *this;
        }
        CreateQuery& table(const std::string_view table_name) {
            table_name_ = table_name.data();
            return *this;
        }
        [[nodiscard]] const Columns& get_columns() const noexcept {
            return columns_;
        }
        [[nodiscard]] std::string_view get_table_name() const noexcept {
            return table_name_;
        }

    private:
        std::string table_name_;
        Columns columns_;
    };

    class CountQuery final : public TableContext<CountQuery>,
                             public WhereContext<CountQuery>,
                             public QueryUtilities<CountQuery> {};

    class TruncateQuery final : public TableContext<TruncateQuery> {};
} // namespace demiplane::database::query
