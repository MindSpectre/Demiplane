// db_conditions.hpp

#pragma once

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "db_field.hpp"
#include "db_shortcuts.hpp"
#include <db_factory.hpp>
namespace common::database {
    class FieldCondition {
    public:
        enum class Operator { EQUAL, GREATER_THAN, LESS_THAN, GREATER_THAN_OR_EQUAL, LESS_THAN_OR_EQUAL, NOT_EQUAL };
        ~FieldCondition()                                    = default;
        FieldCondition(FieldCondition&&) noexcept            = default;
        FieldCondition& operator=(FieldCondition&&) noexcept = default;
        FieldCondition(const FieldCondition&)                = delete;
        FieldCondition& operator=(const FieldCondition&)     = delete;

        FieldCondition(const UniqueFieldPtr& field, const Operator op, UniqueFieldPtr value) {
            value_ = std::move(value);
            value_->set_name(std::move(field->get_name()));
            operator_ = op;
        }
        FieldCondition(UniqueFieldPtr field, const Operator op) {
            value_    = std::move(field);
            operator_ = op;
        }
        template <typename T1>
        FieldCondition(std::string name, const Operator op, T1 value) : operator_{op} {
            value_ = utility_factory::unique_field<T1>(std::move(name), value);
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
        UniqueFieldPtr value_;
    };

    class PatternCondition final {
    public:
        explicit PatternCondition(std::string&& pattern) : pattern_(std::move(pattern)) {}

        explicit PatternCondition(const std::string& pattern) : pattern_(pattern) {}

        ~PatternCondition() = default;

        [[nodiscard]] const std::string& get_pattern() const& {
            return pattern_;
        }

        void set_pattern(std::string&& pattern) & {
            pattern_ = std::move(pattern);
        }

        void set_pattern(const std::string& pattern) & {
            pattern_ = pattern;
        }

    protected:
        std::string pattern_;
    };

    enum class order_type { ascending, descending };

    class OrderCondition final {
    public:
        ~OrderCondition()                                    = default;
        OrderCondition(OrderCondition&&) noexcept            = default;
        OrderCondition& operator=(OrderCondition&&) noexcept = default;
        OrderCondition(const OrderCondition&)                = delete;
        OrderCondition& operator=(const OrderCondition&)     = delete;

        explicit OrderCondition(
            UniqueFieldPtr&& field, const order_type order = order_type::ascending, std::string spec_ = {})
            : column_(std::move(field)), order_(order), specifier_(std::move(spec_)) {}

        explicit OrderCondition(
            std::string column_name, const order_type order = order_type::ascending, std::string spec_ = {})
            : order_(order), specifier_(std::move(spec_)) {
            column_ = std::make_unique<Field<int32_t>>(std::move(column_name), 0);
        }

        [[nodiscard]] const UniqueFieldPtr& get_column() const {
            return column_;
        }

        void set_column(UniqueFieldPtr&& column) & {
            column_ = std::move(column);
        }

        [[nodiscard]] order_type get_order() const {
            return order_;
        }

        void set_order(const order_type order) & {
            order_ = order;
        }

        [[nodiscard]] const std::string& get_specifier() const {
            return specifier_;
        }

        void set_specifier(const std::string& specifier) & {
            specifier_ = specifier;
        }

        void set_specifier(std::string&& specifier) & {
            specifier_ = std::move(specifier);
        }

    private:
        UniqueFieldPtr column_;
        order_type order_ = order_type::ascending;
        std::string specifier_;
    };

    class SimilarityCondition final {
    public:
        ~SimilarityCondition()                                         = default;
        SimilarityCondition(SimilarityCondition&&) noexcept            = default;
        SimilarityCondition& operator=(SimilarityCondition&&) noexcept = default;
        SimilarityCondition(const SimilarityCondition&)                = default;
        SimilarityCondition& operator=(const SimilarityCondition&)     = default;

        explicit SimilarityCondition(std::string pattern) : pattern_(std::move(pattern)) {}


        [[nodiscard]] std::string get_pattern() const {
            return pattern_;
        }

    private:
        std::string pattern_;
    };

    class PageCondition final {
    public:
        ~PageCondition() = default;
        // Constructors
        explicit PageCondition(const int32_t limit, const int32_t offset = 0) : limit_(limit), offset_(offset) {}

        // Accessors
        [[nodiscard]] uint32_t get_limit() const& {
            return limit_;
        }

        [[nodiscard]] uint32_t get_offset() const& {
            return offset_;
        }

        // Modifiers
        void set_limit(const uint32_t limit) & {
            limit_ = limit;
        }

        PageCondition& set_limit(const uint32_t limit) && {
            limit_ = limit;
            return *this;
        }

        /// @warning accept 1-indexing
        PageCondition& set_page_number(const uint32_t page_number) && {
            offset_ = limit_ * (page_number - 1);
            return *this;
        }

        void set_page_number(const int32_t page_number) & {
            offset_ = limit_ * (page_number - 1);
        }

        void set_offset(const uint32_t offset) {
            offset_ = offset;
        }

    private:
        uint32_t limit_; // Number of records per page
        uint32_t offset_; // Starting point in the dataset (calculated as page_number * limit)
    };

    class Conditions final {
    public:
        ~Conditions() = default;

        void add_field_condition(FieldCondition&& condition) & {
            conditions_.push_back(std::move(condition));
        }

        template <typename... Args>
        void add_field_condition(Args&&... args) & {
            conditions_.emplace_back(std::forward<Args>(args)...);
        }

        void add_pattern_condition(PatternCondition&& condition) & {
            patterns_.push_back(std::move(condition));
        }

        template <typename... Args>
        void add_pattern_condition(Args&&... args) & {
            patterns_.emplace_back(std::forward<Args>(args)...);
        }

        void add_similarity_condition(SimilarityCondition&& condition) & {
            similarity_conditions_.push_back(std::move(condition));
        }

        template <typename... Args>
        void add_similarity_condition(Args&&... args) & {
            similarity_conditions_.emplace_back(std::forward<Args>(args)...);
        }

        void add_order_by_condition(OrderCondition&& condition) & {
            orders_.push_back(std::move(condition));
        }

        template <typename... Args>
        void add_order_by_condition(Args&&... args) & {
            orders_.emplace_back(std::forward<Args>(args)...);
        }


        template <typename... Args>
        void set_page_condition(Args&&... args) & {
            pages_ = PageCondition(std::forward<Args>(args)...);
        }


        void set_page_condition(PageCondition condition) & {
            pages_ = condition;
        }

        void pop_field_condition() & {
            conditions_.pop_back();
        }

        void pop_pattern_condition() & {
            patterns_.pop_back();
        }

        void pop_order_by_condition() & {
            orders_.pop_back();
        }

        void pop_similarity_condition() & {
            similarity_conditions_.pop_back();
        }

        void clear_field_conditions() & {
            conditions_.clear();
        }

        void clear_pattern_conditions() & {
            patterns_.clear();
        }

        void clear_order_by_conditions() & {
            orders_.clear();
        }

        void clear_page_conditions() & {
            pages_.reset();
        }

        void clear_similarity_conditions() & {
            similarity_conditions_.clear();
        }

        [[nodiscard]] const std::list<FieldCondition>& fields_conditions() const& {
            return conditions_;
        }

        [[nodiscard]] const std::list<PatternCondition>& pattern_conditions() const& {
            return patterns_;
        }

        [[nodiscard]] const std::list<OrderCondition>& order_by_conditions() const& {
            return orders_;
        }

        [[nodiscard]] const std::optional<PageCondition>& page_condition() const& {
            return pages_;
        }

        [[nodiscard]] const std::list<SimilarityCondition>& similarity_conditions() const& {
            return similarity_conditions_;
        }

        [[nodiscard]] bool empty() const {
            return conditions_.empty() && patterns_.empty() && orders_.empty() && similarity_conditions_.empty()
                && !pages_.has_value();
        }

    private:
        std::list<FieldCondition> conditions_;
        std::list<PatternCondition> patterns_;
        std::list<SimilarityCondition> similarity_conditions_;
        std::list<OrderCondition> orders_;
        std::optional<PageCondition> pages_;
    };
} // namespace common::database
