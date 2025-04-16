#pragma once

#include <chrono>
#include <format> // C++23: for std::chrono::format
#include <json/json.h>
#include <memory>
#include <ostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <demiplane/gears>
namespace demiplane::database {

    // The SQL type enumeration.
    enum class SqlType {
        INT,
        ARRAY_INT,
        UUID,
        PRIMARY_UUID,
        NULL_UUID,
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
    template <typename T>
    class Field;
    // A class representing a UUID field.
    class Uuid {
    public:
        static constexpr auto use_generated = "use_generated";
        static constexpr auto null_value    = "null";

        Uuid() = default;
        explicit Uuid(std::string value, const bool is_primary = true) : primary_{is_primary}, uuid_(std::move(value)) {
            is_null_   = (uuid_ == null_value);
            generated_ = (!is_null_ && uuid_ == use_generated);
            if (!is_valid_uuid(uuid_)) {
                throw std::invalid_argument("Uuid is not valid.");
            }
        }

        Uuid& operator=(std::string other) {
            uuid_      = std::move(other);
            is_null_   = (uuid_ == null_value);
            generated_ = (uuid_ == use_generated);
            return *this;
        }

        [[nodiscard]] bool is_null() const noexcept {
            return is_null_;
        }
        [[nodiscard]] bool is_generated() const noexcept {
            return generated_;
        }
        [[nodiscard]] bool is_primary() const noexcept {
            return primary_;
        }

        Uuid& set_generated() noexcept {
            generated_ = true;
            is_null_   = false;
            return *this;
        }
        Uuid& set_null() noexcept {
            uuid_      = null_value;
            is_null_   = true;
            generated_ = false;
            primary_   = false;
            return *this;
        }
        Uuid& set_primary() noexcept {
            primary_ = true;
            is_null_ = false;
            return *this;
        }

        Uuid& unset_generated() noexcept {
            generated_ = false;
            return *this;
        }
        Uuid& unset_null() noexcept {
            uuid_      = use_generated;
            is_null_   = false;
            generated_ = true;
            return *this;
        }
        Uuid& unset_primary() noexcept {
            primary_ = false;
            return *this;
        }

        [[nodiscard]] const std::string& get_id() const noexcept {
            return uuid_;
        }
        [[nodiscard]] std::string pull_id() {
            return std::move(uuid_);
        }

        void set_id(const std::string_view uuid) {
            if (uuid.empty()) {
                throw std::invalid_argument("Uuid cannot be empty.");
            }
            if (!is_valid_uuid(uuid)) {
                throw std::invalid_argument("Uuid is not valid.");
            }
            uuid_      = uuid;
            is_null_   = (uuid_ == null_value);
            generated_ = (uuid_ == use_generated);
        }

        explicit operator std::string() const {
            return uuid_;
        }

        bool operator==(const Uuid& other) const noexcept {
            return uuid_ == other.uuid_;
        }
        bool operator!=(const Uuid& other) const noexcept {
            return !(*this == other);
        }
        friend bool operator<(const Uuid& lhs, const Uuid& rhs) noexcept {
            return lhs.uuid_ < rhs.uuid_;
        }
        friend bool operator<=(const Uuid& lhs, const Uuid& rhs) noexcept {
            return rhs >= lhs;
        }
        friend bool operator>(const Uuid& lhs, const Uuid& rhs) noexcept {
            return rhs < lhs;
        }
        friend bool operator>=(const Uuid& lhs, const Uuid& rhs) noexcept {
            return !(lhs < rhs);
        }

        friend std::ostream& operator<<(std::ostream& os, const Uuid& obj) {
            return os << obj.uuid_;
        }

        // Validate UUID format (a basic check – can be extended).
        static bool is_valid_uuid(const std::string_view value) {
            if (value == use_generated || value == null_value) {
                return true;
            }
            static const std::regex uuid_regex{
                R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})"};
            // (Constructing a std::string here is one simple way to feed regex_match.)
            return std::regex_match(std::string(value).c_str(), uuid_regex);
        }

    private:
        bool primary_   = true;
        bool generated_ = true;
        bool is_null_   = false;
        std::string uuid_{use_generated};
    };
    // --- Helpers for converting field values to SQL string representations ---
    namespace detail {


        // convert_value: converts any supported type to its SQL string representation.
        // This single function template uses perfect forwarding so that if the argument
        // is a rvalue (e.g. from a moved Field), it may “pull” the value.
        template <typename U>
        std::string convert_value(U&& value) {
            using T = std::remove_cvref_t<U>;
            if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> || std::is_same_v<T, const char*>) {
                if constexpr (std::is_lvalue_reference_v<U>) {
                    return value;
                } else {
                    return std::forward<U>(value);
                }
            } else if constexpr (std::is_same_v<T, bool>) {
                return value ? "TRUE" : "FALSE";
            } else if constexpr (std::is_arithmetic_v<T>) {
                return std::to_string(value);
            } else if constexpr (std::is_same_v<T, Uuid>) {
                if constexpr (std::is_lvalue_reference_v<U>) {
                    return value.get_id();
                } else {
                    return value.pull_id();
                }
            } else if constexpr (std::is_same_v<T, Json::Value>) {
                return value.toStyledString();
            } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
                return std::format("{:%Y-%m-%d %X}", value);
            } else if constexpr (is_vector_v<T>) {
                using V = typename T::value_type;
                std::ostringstream oss;
                oss << "ARRAY[";
                bool first = true;
                for (auto&& elem : value) {
                    if (!first) {
                        oss << ", ";
                    }
                    first = false;
                    if constexpr (std::is_same_v<V, bool>) {
                        oss << convert_value(static_cast<bool>(elem));
                    } else if constexpr (std::is_same_v<V, Uuid>) {
                        // In an array of UUIDs, throw if any element is “default/primary”
                        if (elem.is_primary()) {
                            throw std::invalid_argument("Arrays cannot contain primary keys.");
                        }
                        if (elem.is_null()) {
                            throw std::invalid_argument(
                                "Arrays cannot contain null ids. Null key element could be removed.");
                        }
                        if (elem.is_generated()) {
                            throw std::invalid_argument("For array field received uuid without value. Array does not "
                                                        "support db generation elements.");
                        }
                        if constexpr (std::is_lvalue_reference_v<decltype(elem)>) {
                            oss << elem.get_id();
                        } else {
                            oss << elem.pull_id();
                        }
                    } else {
                        oss << convert_value(elem);
                    }
                }
                oss << "]";
                return oss.str();
            } else {
                static_assert(always_false_v<U>, "Unsupported field type for SQL conversion");
            }
            return {};
        }

        // Map a type T to its SqlType.
        template <typename T>
        constexpr SqlType deduce_sql_type(const T& value) {
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
                return SqlType::INT;
            } else if constexpr (std::is_same_v<T, Uuid>) {
                if (value.is_primary()) {
                    return SqlType::PRIMARY_UUID;
                }
                if (value.is_null()) {
                    return SqlType::NULL_UUID;
                }
                return SqlType::UUID;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return SqlType::BIGINT;
            } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
                return SqlType::DOUBLE_PRECISION;
            } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> || std::is_same_v<T, const char*>) {
                return SqlType::TEXT;
            } else if constexpr (std::is_same_v<T, Json::Value>) {
                return SqlType::JSONB;
            } else if constexpr (std::is_same_v<T, bool>) {
                return SqlType::BOOLEAN;
            } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
                return SqlType::TIMESTAMP;
            } else if constexpr (is_vector_v<T>) {
                using V = typename T::value_type;
                if constexpr (std::is_same_v<V, int> || std::is_same_v<V, int32_t>) {
                    return SqlType::ARRAY_INT;
                } else if constexpr (std::is_same_v<V, Uuid>) {
                    return SqlType::ARRAY_UUID;
                } else if constexpr (std::is_same_v<V, int64_t>) {
                    return SqlType::ARRAY_BIGINT;
                } else if constexpr (std::is_same_v<V, double> || std::is_same_v<V, float>) {
                    return SqlType::ARRAY_DOUBLE;
                } else if constexpr (std::is_same_v<V, std::string> || std::is_same_v<V, std::string_view> || std::is_same_v<V, const char*>) {
                    return SqlType::ARRAY_TEXT;
                } else if constexpr (std::is_same_v<V, bool>) {
                    return SqlType::ARRAY_BOOLEAN;
                } else if constexpr (std::is_same_v<V, std::chrono::system_clock::time_point>) {
                    return SqlType::ARRAY_TIMESTAMP;
                } else {
                    static_assert(always_false_v<T>, "Unsupported vector field type");
                }
            } else {
                static_assert(always_false_v<T>, "Unsupported field type");
            }
            return {};
        }

        // Get a string for the SQL initialization (e.g. in a CREATE TABLE statement).
        inline std::string get_sql_init_type(const SqlType type) {
            switch (type) {
            case SqlType::NULL_UUID:
                return "UUID NULL";
            case SqlType::UUID:
                return "UUID NOT NULL";
            case SqlType::PRIMARY_UUID:
                return "UUID DEFAULT gen_random_uuid() PRIMARY KEY";
            case SqlType::INT:
                return "INT";
            case SqlType::BIGINT:
                return "BIGINT";
            case SqlType::DOUBLE_PRECISION:
                return "DOUBLE PRECISION";
            case SqlType::TEXT:
                return "TEXT";
            case SqlType::JSONB:
                return "JSONB";
            case SqlType::BOOLEAN:
                return "BOOLEAN";
            case SqlType::TIMESTAMP:
                return "TIMESTAMP";
            case SqlType::ARRAY_UUID:
                return "UUID[] NULL";
            case SqlType::ARRAY_INT:
                return "INT[]";
            case SqlType::ARRAY_BIGINT:
                return "BIGINT[]";
            case SqlType::ARRAY_DOUBLE:
                return "DOUBLE PRECISION[]";
            case SqlType::ARRAY_TEXT:
                return "TEXT[]";
            case SqlType::ARRAY_BOOLEAN:
                return "BOOLEAN[]";
            case SqlType::ARRAY_TIMESTAMP:
                return "TIMESTAMP[]";
            case SqlType::UNSUPPORTED:
                throw std::runtime_error("Unsupported field type");
            }
            throw std::runtime_error("Undefined field type. Possible leaks.");
        }

    } // namespace detail

    // Base class for all fields.
    class FieldBase {
    public:
        virtual ~FieldBase() = default;

        explicit FieldBase(const std::string_view name) : name_(name) {}
        explicit FieldBase(std::string name) : name_(std::move(name)) {}

        [[nodiscard]] const std::string& get_name() const noexcept {
            return name_;
        }
        void set_name(const std::string_view name) {
            name_ = name;
        }
        void set_name(std::string name) {
            name_ = std::move(name);
        }
        void set_name(const char* name) {
            name_ = name;
        }

        // Convert the field value to a string for SQL queries.
        [[nodiscard]] virtual std::string to_string() const&  = 0;
        [[nodiscard]] virtual std::string pull_to_string() = 0;

        // Return the SQL type (as determined in the constructor).
        [[nodiscard]] virtual SqlType get_sql_type() const {
            return type_;
        }

        // Return the SQL type for use in CREATE TABLE statements.
        [[nodiscard]] std::string get_sql_type_initialization() const {
            return detail::get_sql_init_type(type_);
        }

        // Helper to cast to the derived field type.
        template <typename T>
        T as() const {
            static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
            if (const auto* derived = dynamic_cast<const Field<T>*>(this)) {
                return derived->value();
            }
            throw std::runtime_error("FieldBase::as(): Incorrect type requested for field " + get_name());
        }

        // For cloning a field.
        [[nodiscard]] virtual std::unique_ptr<FieldBase> clone() const = 0;

    protected:
        SqlType type_{SqlType::UNSUPPORTED};
        std::string name_;
    };


    // Field is a template representing a concrete field with a value.
    template <typename T>
    class Field final : public FieldBase {
    public:
        Field(std::string name, T value) : FieldBase(std::move(name)), value_(std::move(value)) {
            type_ = detail::deduce_sql_type<T>(value_);
        }
        [[nodiscard]] const T& value() const noexcept {
            return value_;
        }
        [[nodiscard]] T& value() noexcept {
            return value_;
        }
        void set_value(const T& value) {
            value_ = value;
        }
        void set_value(T&& value) {
            value_ = std::move(value);
        }

        [[nodiscard]] std::string to_string() const& override {
            return detail::convert_value(value_);
        }

        [[nodiscard]] std::string pull_to_string() override {
            return detail::convert_value(std::move(value_));
        }


        [[nodiscard]] std::unique_ptr<FieldBase> clone() const override {
            return std::make_unique<Field>(*this);
        }

    private:
        T value_;
    };


} // namespace demiplane::database
