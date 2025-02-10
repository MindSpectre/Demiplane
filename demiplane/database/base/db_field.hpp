#pragma once

#include <chrono>
#include <iomanip>
#include <json/json.h>
#include <memory>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace common::database {
    template <typename T>
    class Field;
    /// @brief Base class representing a field in the database
    enum class SqlType {
        INT,
        ARRAY_INT,
        UUID,
        ARRAY_UUID,
        BIGINT,
        ARRAY_BIGINT,
        DOUBLE_PRECISION,
        ARRAY_DOUBLE,
        TEXT,
        ARRAY_TEXT,
        BOOLEAN,
        ARRAY_BOOLEAN,
        TIMESTAMP,
        ARRAY_TIMESTAMP,
        JSONB,
        UNSUPPORTED
    };

    /**
     * @brief On default created PRIMARY uuid generated on database(default type)
     * @details Null means this field can be empty. Default means generation on database side.
            If field is not null or default it forces to set value on server side because of field created as [UUID NOT
     NULL]
     */
    class Uuid {
    public:
        static constexpr auto default_value = "default";
        static constexpr auto null_value    = "null";


        [[nodiscard]] bool is_null() const {
            return is_null_;
        }

        [[nodiscard]] bool is_default() const {
            return is_default_;
        }
        Uuid& set_default() {
            is_default_ = true;
            is_null_    = false;
            return *this;
        }
        Uuid& set_null() {
            uuid_       = null_value;
            is_null_    = true;
            is_default_ = false;
            primary_    = false;
            return *this;
        }

        [[nodiscard]] const std::string& get_id() const {
            return uuid_;
        }
        [[nodiscard]] std::string pull_id() {
            return std::move(uuid_);
        }
        void set_id(const std::string_view uuid) {
            if (uuid.empty()) {
                throw std::invalid_argument("Uuid cannot be empty");
            }
            if (!is_valid_uuid(uuid)) {
                throw std::invalid_argument("Uuid is not valid");
            }
            uuid_       = uuid;
            is_null_    = uuid_ == null_value;
            is_default_ = uuid_ == default_value;
        }
        [[nodiscard]] bool is_primary() const {
            return primary_;
        }

        Uuid& set_primary(const bool is_primary) {
            primary_ = is_primary;
            return *this;
        }

        // Equality operators
        bool operator==(const Uuid& other) const {
            return uuid_ == other.uuid_;
        }

        bool operator!=(const Uuid& other) const {
            return uuid_ != other.uuid_;
        }

        explicit operator std::string() const {
            return uuid_;
        }

        explicit operator std::string() {
            return uuid_;
        }

        // Default value, primary, value will be generated on DB server
        Uuid() = default;

        // Constructor from std::string
        explicit Uuid(std::string value, const bool is_primary) : primary_(is_primary), uuid_(std::move(value)) {
            is_null_    = uuid_ == null_value;
            is_default_ = !is_null_ && uuid_ == default_value;
        }
        Uuid& operator=(std::string other) {
            uuid_ = std::move(other);
            return *this;
        }

        friend bool operator<(const Uuid& lhs, const Uuid& rhs) {
            return lhs.uuid_ < rhs.uuid_;
        }

        friend bool operator<=(const Uuid& lhs, const Uuid& rhs) {
            return rhs >= lhs;
        }

        friend bool operator>(const Uuid& lhs, const Uuid& rhs) {
            return rhs < lhs;
        }

        friend bool operator>=(const Uuid& lhs, const Uuid& rhs) {
            return !(lhs < rhs);
        }

        friend std::ostream& operator<<(std::ostream& os, const Uuid& obj) {
            return os << obj.uuid_;
        }

    private:
        bool primary_     = true;
        bool is_default_  = true;
        bool is_null_     = false;
        std::string uuid_ = default_value;

        // Validate UUID format (basic example, can be extended)
        static bool is_valid_uuid(const std::string_view value) {
            const std::regex uuid_regex(
                R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
            return value == default_value || value == null_value || std::regex_match(value.data(), uuid_regex);
        }
    };

    class FieldBase {
    public:
        virtual ~FieldBase() = default;
        explicit FieldBase(const std::string_view name) : name_(name) {}
        explicit FieldBase(std::string&& name) : name_(std::move(name)) {}
        /// @return Column name of the field
        [[nodiscard]] const std::string& get_name() const {
            return name_;
        }

        void set_name(const std::string_view name) {
            name_ = name;
        }

        void set_name(std::string&& name) {
            name_ = std::move(name);
        }
        void set_name(const char* name) {
            name_ = name;
        }
        /// @brief Converts the field value to a string for SQL queries
        [[nodiscard]] virtual std::string to_string() const& = 0;

        /// @brief Converts the field value to a string for SQL queries
        /// @return moveable
        [[nodiscard]] virtual std::string to_string() && = 0;

        /// @brief Gets the SQL data type of the field
        [[nodiscard]] virtual SqlType get_sql_type() const {
            return type_;
        }

        /// @brief Gets the SQL data type of the field for creating
        [[nodiscard]] virtual constexpr const char* get_sql_type_initialization() const = 0;

        template <typename T>
        T as() const {
            // Ensure that T is a valid type, for example, int, std::string, etc.
            static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

            // Attempt to dynamic cast this object to a Field<T> type

            if (const auto* derived_field = dynamic_cast<const Field<T>*>(this)) {
                return derived_field->value();
            }
            throw std::runtime_error("FieldBase::as(): Incorrect type requested for field " + get_name());
        }

        [[nodiscard]] virtual std::unique_ptr<FieldBase> clone() const = 0;

    protected:
        SqlType type_ = SqlType::UNSUPPORTED;
        std::string name_;
    };

    /// @brief Represents a field of a specific type in the database
    /// @tparam T Type of the value in the database
    template <typename T>
    class Field final : public FieldBase {
    public:
        Field(std::string name, T value) : FieldBase(std::move(name)), value_(std::move(value)) {
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
                type_ = SqlType::INT;
            } else if constexpr (std::is_same_v<T, Uuid>) {
                type_ = SqlType::UUID;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                type_ = SqlType::BIGINT;
            } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
                type_ = SqlType::DOUBLE_PRECISION;
            } else if constexpr (std::is_same_v<T, std::string>) {
                type_ = SqlType::TEXT;
            } else if constexpr (std::is_same_v<T, Json::Value>) {
                type_ = SqlType::JSONB;
            } else if constexpr (std::is_same_v<T, bool>) {
                type_ = SqlType::BOOLEAN;
            } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
                type_ = SqlType::TIMESTAMP;
            } else if constexpr (std::is_same_v<T, std::vector<int>> || std::is_same_v<T, std::vector<int32_t>>) {
                type_ = SqlType::ARRAY_INT;
            } else if constexpr (std::is_same_v<T, std::vector<Uuid>>) {
                type_ = SqlType::ARRAY_UUID;
            } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                type_ = SqlType::ARRAY_BIGINT;
            } else if constexpr (std::is_same_v<T, std::vector<double>> || std::is_same_v<T, std::vector<float>>) {
                type_ = SqlType::ARRAY_DOUBLE;
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                type_ = SqlType::ARRAY_TEXT;
            } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
                type_ = SqlType::ARRAY_BOOLEAN;
            } else if constexpr (std::is_same_v<T, std::vector<std::chrono::system_clock::time_point>>) {
                type_ = SqlType::ARRAY_TIMESTAMP;
            } else {
                static_assert(sizeof(T) == 0, "Unsupported field type for get_sql_type()");
                type_ = SqlType::UNSUPPORTED; // Fallback for unsupported types
            }
        }

        /// @return Value of the field
        const T& value() const {
            return value_;
        }

        void set_value(const T& value) {
            value_ = value;
        }

        void set_value(T&& value) {
            value_ = std::move(value);
        }

        /// @brief Converts the field value to a string for SQL queries
        [[nodiscard]] std::string to_string() const& override {
            if constexpr (std::is_same_v<T, std::string>) {
                return value_;
            } else if constexpr (std::is_same_v<T, bool>) {
                return value_ ? "TRUE" : "FALSE";
            } else if constexpr (std::is_arithmetic_v<T>) {
                return std::to_string(value_);
            } else if constexpr (std::is_same_v<T, Uuid>) {
                return value_.get_id();
            } else if constexpr (std::is_same_v<T, Json::Value>) {
                return value_.toStyledString();
            } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
                // Convert time_point to string in ISO 8601 format
                const std::time_t time = std::chrono::system_clock::to_time_t(value_);
                const std::tm* tm_ptr  = std::localtime(&time);
                std::ostringstream oss;
                oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");
                return oss.str();
            } else if constexpr (std::is_same_v<T, std::vector<int>> || std::is_same_v<T, std::vector<int32_t>>) {
                return array_to_string<int>();
            } else if constexpr (std::is_same_v<T, std::vector<Uuid>>) {
                return array_to_string<Uuid>();
            } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                return array_to_string<int64_t>();
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                return array_to_string<double>();
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                return array_to_string<float>();
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                return array_to_string<std::string>();
            } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
                return array_to_string<bool>();
            } else if constexpr (std::is_same_v<T, std::vector<std::chrono::system_clock::time_point>>) {
                return array_to_string<std::chrono::system_clock::time_point>();
            } else {
                static_assert(sizeof(T) == 0, "Unsupported field type for to_string()");
            }
            return {};
        }

        /// @brief Converts the field value to a string for SQL queries
        [[nodiscard]] std::string to_string() && override {


            switch (type_) {

            }
                if constexpr (std::is_same_v<T, std::string>) {
                    return std::move(value_); // Move the string instead of copying
                } else if constexpr (std::is_same_v<T, bool>) {
                    return value_ ? "TRUE" : "FALSE";
                } else if constexpr (std::is_arithmetic_v<T>) {
                    return std::to_string(value_); // No need to move arithmetic types
                } else if constexpr (std::is_same_v<T, Uuid>) {
                    return value_.pull_id();
                } else if constexpr (std::is_same_v<T, Json::Value>) {
                    return value_.toStyledString(); // Move the JSON string representation
                } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
                    // Convert time_point to string in ISO 8601 format
                    const std::time_t time = std::chrono::system_clock::to_time_t(value_);
                    const std::tm* tm_ptr  = std::localtime(&time);
                    std::ostringstream oss;
                    oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");
                    return oss.str(); // No need to move, it's a local object
                } else if constexpr (std::is_same_v<T, std::vector<int>> || std::is_same_v<T, std::vector<int32_t>>) {
                    return array_move_to_string<int>();
                } else if constexpr (std::is_same_v<T, std::vector<Uuid>>) {
                    return array_move_to_string<Uuid>();
                } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                    return array_move_to_string<int64_t>();
                } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                    return array_move_to_string<double>();
                } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                    return array_move_to_string<float>();
                } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                    return array_move_to_string<std::string>();
                } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
                    return array_move_to_string<bool>();
                } else if constexpr (std::is_same_v<T, std::vector<std::chrono::system_clock::time_point>>) {
                    return array_move_to_string<std::chrono::system_clock::time_point>();
                } else {
                    static_assert(sizeof(T) == 0, "Unsupported field type for to_string()");
                }
            return {};
        }


        /// @brief Gets the SQL data type of the field
        [[nodiscard]] constexpr const char* get_sql_type_initialization() const override {
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
                return "INT";
            } else if constexpr (std::is_same_v<T, Uuid>) {
                if (this->value().is_primary()) {
                    return "UUID DEFAULT gen_random_uuid() PRIMARY KEY";
                }
                if (this->value().is_null()) {
                    return "UUID NULL";
                }
                return "UUID NOT NULL";
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return "BIGINT";
            } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
                return "DOUBLE PRECISION";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return "TEXT";
            } else if constexpr (std::is_same_v<T, Json::Value>) {
                return "JSONB";
            } else if constexpr (std::is_same_v<T, bool>) {
                return "BOOLEAN";
            } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
                return "TIMESTAMP";
            } else if constexpr (std::is_same_v<T, std::vector<int>> || std::is_same_v<T, std::vector<int32_t>>) {
                return "INT[]";
            } else if constexpr (std::is_same_v<T, std::vector<Uuid>>) {
                return "UUID[] NULL";
            } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                return "BIGINT[]";
            } else if constexpr (std::is_same_v<T, std::vector<double>> || std::is_same_v<T, std::vector<float>>) {
                return "DOUBLE PRECISION[]";
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                return "TEXT[]";
            } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
                return "BOOLEAN[]";
            } else if constexpr (std::is_same_v<T, std::vector<std::chrono::system_clock::time_point>>) {
                return "TIMESTAMP[]";
            } else {
                static_assert(sizeof(T) == 0, "Unsupported field type for get_sql_type()");
            }
            return {};
        }

        [[nodiscard]] std::unique_ptr<FieldBase> clone() const override {
            return std::make_unique<Field>(*this);
        }

    private:
        template <typename S>
        [[nodiscard]] std::string array_to_string() const {
            std::ostringstream oss;
            oss << "ARRAY[ ";
            for (size_t i = 0; i < value_.size(); i++) {
                if constexpr (std::is_same_v<S, bool>) {
                    oss << (value_[i] ? "TRUE" : "FALSE");
                } else if constexpr (std::is_same_v<S, std::string> || std::is_arithmetic_v<S>) {
                    oss << value_[i]; // Move the string instead of copying
                } else if constexpr (std::is_same_v<S, Uuid>) {
                    if (value_[i].is_primary() || value_[i].is_null() || value_[i].is_default()) {
                        throw std::runtime_error("For array field received uuid without value");
                    }
                    oss << value_[i].get_id();
                } else if constexpr (std::is_same_v<S, std::chrono::system_clock::time_point>) {
                    // Convert time_point to string in ISO 8601 format
                    const std::time_t time = std::chrono::system_clock::to_time_t(value_[i]);
                    const std::tm* tm_ptr  = std::localtime(&time);
                    oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");
                } else {
                    static_assert(sizeof(T) == 0, "Unsupported field type for to_string()");
                }
                if (i < value_.size() - 1) {
                    oss << ", ";
                }
            }

            oss << "]";
            return oss.str();
        }
        template <typename S>
        [[nodiscard]] std::string array_move_to_string() {
            std::ostringstream oss;
            oss << "ARRAY[";
            for (size_t i = 0; i < value_.size(); i++) {
                if constexpr (std::is_same_v<S, std::string>) {
                    oss << std::move(value_[i]); // Move the string instead of copying
                } else if constexpr (std::is_same_v<S, bool>) {
                    oss << (value_[i] ? "TRUE" : "FALSE");
                } else if constexpr (std::is_arithmetic_v<S>) {
                    oss << value_[i]; // No need to move arithmetic types
                } else if constexpr (std::is_same_v<S, Uuid>) {
                    if (value_[i].is_primary() || value_[i].is_null() || value_[i].is_default()) {
                        throw std::runtime_error("For array field received uuid without value");
                    }
                    oss << value_[i].pull_id();
                } else if constexpr (std::is_same_v<S, std::chrono::system_clock::time_point>) {
                    // Convert time_point to string in ISO 8601 format
                    const std::time_t time = std::chrono::system_clock::to_time_t(value_[i]);
                    const std::tm* tm_ptr  = std::localtime(&time);
                    oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");
                } else {
                    static_assert(sizeof(S) == 0, "Unsupported field type for to_string()");
                }
                if (i < value_.size() - 1) {
                    oss << ", ";
                }
            }

            oss << "]";
            return oss.str();
        }
        T value_;
    };


    class ViewingField final : public FieldBase {
    public:
        ViewingField(std::string&& name, const std::string_view value) : FieldBase(std::move(name)), value_(value) {}


        /// @return Value of the field
        [[nodiscard]] std::string_view value() const {
            return value_;
        }

        void set_value(const std::string_view& value) {
            value_ = value;
        }

        void set_value(std::string_view&& value) {
            value_ = value;
        }

        /// @brief Converts the field value to a string for SQL queries
        [[nodiscard]] std::string to_string() const& override {
            return std::string(value_);
        }

        /// @brief Converts the field value to a string for SQL queries
        [[nodiscard]] std::string to_string() && override {
            return std::string(value_);
        }

        /// @brief Gets the SQL data type of the field
        [[nodiscard]] SqlType get_sql_type() const override {
            throw std::runtime_error("get_sql_type() called in VIEWING field");
        }

        [[nodiscard]] constexpr const char* get_sql_type_initialization() const override {
            throw std::runtime_error("get_sql_type_initialization() called in VIEWING field");
        }

        [[nodiscard]] std::unique_ptr<FieldBase> clone() const override {
            return std::make_unique<ViewingField>(*this);
        }

    private:
        std::string_view value_;
    };


} // namespace common::database
