#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Include your FieldBase definition (and related Field<> classes) here.
#include "db_field.hpp"

namespace demiplane::database {

class Record final {
public:
    Record() = default;
    ~Record() = default;

    // Allow moves but disable copying. (Use clone() for deep copying.)
    Record(Record&& other) noexcept : fields_(std::move(other.fields_)) {}
    Record& operator=(Record&& other) noexcept {
        if (this != &other)
            fields_ = std::move(other.fields_);
        return *this;
    }
    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    /// @brief Produce a deep copy of this record.
    [[nodiscard]] Record clone() const {
        Record copy;
        copy.fields_.reserve(fields_.size());
        for (const auto& field : fields_) {
            if (field) {
                copy.fields_.push_back(field->clone());
            }
        }
        return copy;
    }

    /// @brief Append a field to the record.
    void push_back(std::unique_ptr<FieldBase>&& field) {
        fields_.push_back(std::move(field));
    }

    /// @brief Emplace a new field at the end of the record.
    template <typename FieldType, typename... Args>
    void emplace_back(Args&&... args) {
        static_assert(std::is_base_of_v<FieldBase, FieldType>,
                      "FieldType must derive from FieldBase");
        fields_.emplace_back(std::make_unique<FieldType>(
            std::forward<Args>(args)...));
    }

    /// @brief Remove the last field.
    void pop_back() {
        if (fields_.empty())
            throw std::runtime_error("Record is empty, cannot pop_back");
        fields_.pop_back();
    }

    /// @brief Remove and return the last field.
    [[nodiscard]] std::unique_ptr<FieldBase> pull_back() {
        if (fields_.empty())
            throw std::runtime_error("Record is empty, cannot pull_back");
        auto tmp = std::move(fields_.back());
        fields_.pop_back();
        return tmp;
    }

    /// @brief Remove all fields.
    void clear() noexcept {
        fields_.clear();
    }

    /// @brief Number of fields in this record.
    [[nodiscard]] std::size_t size() const noexcept {
        return fields_.size();
    }

    /// @brief Whether this record has no fields.
    [[nodiscard]] bool empty() const noexcept {
        return fields_.empty();
    }

    /// @brief Reserve capacity for fields.
    void reserve(const std::size_t sz) {
        fields_.reserve(sz);
    }

    /// @brief Access a field by index (with bounds checking).
    const std::unique_ptr<FieldBase>& operator[](const std::size_t idx) const {
        if (idx >= fields_.size())
            throw std::out_of_range("Index out of range in Record");
        return fields_[idx];
    }
    std::unique_ptr<FieldBase>& operator[](const std::size_t idx) {
        if (idx >= fields_.size())
            throw std::out_of_range("Index out of range in Record");
        return fields_[idx];
    }

    // --- Range-based iteration support ---
    auto begin() noexcept { return fields_.begin(); }
    auto end() noexcept { return fields_.end(); }
    [[nodiscard]] auto begin() const noexcept { return fields_.begin(); }
    [[nodiscard]] auto end() const noexcept { return fields_.end(); }
    [[nodiscard]] auto cbegin() const noexcept { return fields_.cbegin(); }
    [[nodiscard]] auto cend() const noexcept { return fields_.cend(); }

    // --- Lookup by field name ---

    /// @brief Find the first field with a matching name.
    /// @returns A pointer to the field (or nullptr if not found).
    [[nodiscard]] FieldBase* find(std::string_view name) {
        const auto it = std::ranges::find_if(
            fields_,
            [name](const std::unique_ptr<FieldBase>& field) {
                return field && field->get_name() == name;
            });
        return it != fields_.end() ? it->get() : nullptr;
    }

    /// @brief Const overload of find().
    [[nodiscard]] const FieldBase* find(std::string_view name) const {
        const auto it = std::ranges::find_if(
            fields_,
            [name](const std::unique_ptr<FieldBase>& field) {
                return field && field->get_name() == name;
            });
        return it != fields_.end() ? it->get() : nullptr;
    }

    /// @brief Convenience operator to access a field by name.
    /// @returns A pointer to the field (or nullptr if not found).
    [[nodiscard]] FieldBase* operator[](const std::string_view name) {
        return find(name);
    }
    [[nodiscard]] const FieldBase* operator[](const std::string_view name) const {
        return find(name);
    }

    // --- Typed value extraction ---

    /// @brief Retrieve the value of the field with the given name, cast to type T.
    /// @throws std::runtime_error if the field is not found.
    template <typename T>
    [[nodiscard]] T get_value(const std::string_view name) const {
        const FieldBase* field = find(name);
        if (!field)
            throw std::runtime_error("Field not found: " + std::string(name));
        return field->as<T>();
    }

private:
    std::vector<std::unique_ptr<FieldBase>> fields_;
};

} // namespace demiplane::database
