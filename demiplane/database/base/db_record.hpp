// db_record.hpp

#pragma once
#include <memory>
#include <string>

#include "db_field.hpp"

namespace common::database {

    class Record final {
    public:
        ~Record() = default;

        void push_back(std::unique_ptr<FieldBase>&& field) {
            fields_.push_back(std::move(field));
        }

        template <typename FieldType, typename... Args>
        void emplace_back(Args&&... args) {
            static_assert(std::is_base_of_v<FieldBase, FieldType>, "FieldType must derive from FieldBase");
            fields_.emplace_back(std::make_unique<FieldType>(std::forward<Args>(args)...));
        }

        void pop_back() {
            fields_.pop_back();
        }

        void clear() {
            fields_.clear();
        }

        Record(Record&& other) noexcept : fields_(std::move(other.fields_)) {}

        // Explicitly define the move assignment operator
        Record& operator=(Record&& other) noexcept {
            if (this != &other) {
                fields_ = std::move(other.fields_);
            }
            return *this;
        }

        // Disable the copy constructor and assignment operator to avoid accidental copying
        Record(const Record&)            = delete;
        Record& operator=(const Record&) = delete;
        /*explicit*/
        Record() = default;

        [[nodiscard]] std::unique_ptr<FieldBase> pull_back() {
            auto tmp = std::move(fields_.back());
            fields_.pop_back();
            return std::move(tmp);
        }

        [[nodiscard]] std::size_t size() const {
            return fields_.size();
        }

        const std::unique_ptr<FieldBase>& operator[](const std::size_t idx) const& {
            return fields_[idx];
        }

        void reserve(const std::size_t sz) {
            fields_.reserve(sz);
        }

        [[nodiscard]] bool empty() const {
            return fields_.empty();
        }

        [[nodiscard]] const std::vector<std::unique_ptr<FieldBase>>& fields() const& {
            return fields_;
        }

        [[nodiscard]] std::vector<std::unique_ptr<FieldBase>> fields() && {
            return std::move(fields_);
        }


        // Implement begin() and end() methods
        auto begin() {
            return fields_.begin();
        }
        auto end() {
            return fields_.end();
        }
        [[nodiscard]] auto begin() const {
            return fields_.cbegin();
        }
        [[nodiscard]] auto end() const {
            return fields_.cend();
        }

    private:
        std::vector<std::unique_ptr<FieldBase>> fields_;
    };


    class ViewRecord {
    public:
        virtual ~ViewRecord()                                               = default;
        [[nodiscard]] virtual std::string_view view(std::size_t idx) const& = 0;
        [[nodiscard]] virtual std::string extract(std::size_t idx) const&   = 0;
        [[nodiscard]] virtual std::size_t size() const&                     = 0;
        [[nodiscard]] virtual std::string name(std::size_t idx) const&      = 0;
    };

    class BaseViewRecord final : public ViewRecord {
    public:
        [[nodiscard]] std::string_view view(const std::size_t idx) const& override {
            return views_[idx].value();
        }

        void add_field(ViewingField&& field) {
            views_.emplace_back(std::move(field));
        }

        [[nodiscard]] std::string extract(const std::size_t idx) const& override {
            return std::string{views_[idx].value()};
        }

        [[nodiscard]] std::size_t size() const& override {
            return views_.size();
        }

        [[nodiscard]] std::string name(const std::size_t idx) const& override {
            return views_[idx].get_name();
        }

    private:
        std::vector<ViewingField> views_;
    };

} // namespace common::database
