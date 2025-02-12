#pragma once

#include "db_field.hpp"
#include "db_record.hpp"
#include "db_shortcuts.hpp"

namespace demiplane::database {
    class utility_factory {
    public:
        template <typename T>
            requires std::default_initializable<T>
        static SharedFieldPtr shared_field(std::string name, T value = T()) {
            return std::make_shared<Field<T>>(std::move(name), std::move(value));
        }

        template <typename T>
            requires std::default_initializable<T>
        static UniqueFieldPtr unique_field(std::string name, T value = T()) {
            return std::make_unique<Field<T>>(std::move(name), std::move(value));
        }
    };

    class shared_field_factory : utility_factory {
    public:
        static SharedFieldPtr text_field(std::string name, std::string text = {}) {
            return std::make_shared<Field<std::string>>(std::move(name), std::move(text));
        }

        static SharedFieldPtr uuid_field(std::string name, Uuid uuid = {}) {
            return std::make_shared<Field<Uuid>>(std::move(name), std::move(uuid));
        }
        static SharedFieldPtr bool_field(std::string name, bool value = {}) {
            return std::make_shared<Field<bool>>(std::move(name), value);
        }
        static SharedFieldPtr double_field(std::string name, double value = {}) {
            return std::make_shared<Field<double>>(std::move(name), value);
        }
        static SharedFieldPtr float_field(std::string name, float value = {}) {
            return std::make_shared<Field<float>>(std::move(name), value);
        }
        static SharedFieldPtr int_field(std::string name, int32_t value = {}) {
            return std::make_shared<Field<int32_t>>(std::move(name), value);
        }
        static SharedFieldPtr ll_int_field(std::string name, int64_t value = {}) {
            return std::make_shared<Field<int64_t>>(std::move(name), value);
        }
        static SharedFieldPtr json_field(std::string name, Json::Value json_value = {}) {
            return std::make_shared<Field<Json::Value>>(std::move(name), std::move(json_value));
        }
        static SharedFieldPtr time_field(std::string name, std::chrono::system_clock::time_point time = {}) {
            return std::make_shared<Field<std::chrono::system_clock::time_point>>(std::move(name), std::move(time));
        }
    };

    class unique_field_factory : utility_factory {
    public:
        static UniqueFieldPtr text_field(std::string name, std::string text = {}) {
            return std::make_unique<Field<std::string>>(std::move(name), std::move(text));
        }

        static UniqueFieldPtr uuid_field(std::string name, Uuid uuid = {}) {
            return std::make_unique<Field<Uuid>>(std::move(name), std::move(uuid));
        }
        static UniqueFieldPtr bool_field(std::string name, bool value = {}) {
            return std::make_unique<Field<bool>>(std::move(name), value);
        }
        static UniqueFieldPtr double_field(std::string name, double value = {}) {
            return std::make_unique<Field<double>>(std::move(name), value);
        }
        static UniqueFieldPtr float_field(std::string name, float value = {}) {
            return std::make_unique<Field<float>>(std::move(name), value);
        }
        static UniqueFieldPtr int_field(std::string name, int32_t value = {}) {
            return std::make_unique<Field<int32_t>>(std::move(name), value);
        }
        static UniqueFieldPtr ll_int_field(std::string name, int64_t value = {}) {
            return std::make_unique<Field<int64_t>>(std::move(name), value);
        }
        static UniqueFieldPtr json_field(std::string name, Json::Value json_value = {}) {
            return std::make_unique<Field<Json::Value>>(std::move(name), std::move(json_value));
        }
        static UniqueFieldPtr time_field(std::string name, std::chrono::system_clock::time_point time = {}) {
            return std::make_unique<Field<std::chrono::system_clock::time_point>>(std::move(name), std::move(time));
        }
    };
} // namespace demiplane::database
