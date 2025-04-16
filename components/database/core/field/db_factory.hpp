#pragma once

#include "db_record.hpp"
namespace demiplane::database {
    class utility_factory {
    public:
        template <typename T>
            requires std::default_initializable<T>
        static std::shared_ptr<FieldBase> shared_field(std::string name, T value = T{}) {
            return std::make_shared<Field<T>>(std::move(name), std::move(value));
        }

        template <typename T>
            requires std::default_initializable<T>
        static std::unique_ptr<FieldBase> unique_field(std::string name, T value = T{}) {
            return std::make_unique<Field<T>>(std::move(name), std::move(value));
        }
    };

    class shared_field_factory : utility_factory {
    public:
        static std::shared_ptr<FieldBase> text_field(std::string name, std::string text = {}) {
            return std::make_shared<Field<std::string>>(std::move(name), std::move(text));
        }

        static std::shared_ptr<FieldBase> uuid_field(std::string name, Uuid uuid = {}) {
            return std::make_shared<Field<Uuid>>(std::move(name), std::move(uuid));
        }
        static std::shared_ptr<FieldBase> bool_field(std::string name, bool value = {}) {
            return std::make_shared<Field<bool>>(std::move(name), value);
        }
        static std::shared_ptr<FieldBase> double_field(std::string name, double value = {}) {
            return std::make_shared<Field<double>>(std::move(name), value);
        }
        static std::shared_ptr<FieldBase> float_field(std::string name, float value = {}) {
            return std::make_shared<Field<float>>(std::move(name), value);
        }
        static std::shared_ptr<FieldBase> int_field(std::string name, int32_t value = {}) {
            return std::make_shared<Field<int32_t>>(std::move(name), value);
        }
        static std::shared_ptr<FieldBase> ll_int_field(std::string name, int64_t value = {}) {
            return std::make_shared<Field<int64_t>>(std::move(name), value);
        }
        static std::shared_ptr<FieldBase> json_field(std::string name, Json::Value json_value = {}) {
            return std::make_shared<Field<Json::Value>>(std::move(name), std::move(json_value));
        }
        static std::shared_ptr<FieldBase> time_field(
            std::string name, std::chrono::system_clock::time_point time = {}) {
            return std::make_shared<Field<std::chrono::system_clock::time_point>>(std::move(name), time);
        }
    };

    class unique_field_factory : utility_factory {
    public:
        static std::unique_ptr<FieldBase> text_field(std::string name, std::string text = {}) {
            return std::make_unique<Field<std::string>>(std::move(name), std::move(text));
        }

        static std::unique_ptr<FieldBase> uuid_field(std::string name, Uuid uuid = {}) {
            return std::make_unique<Field<Uuid>>(std::move(name), std::move(uuid));
        }
        static std::unique_ptr<FieldBase> bool_field(std::string name, bool value = {}) {
            return std::make_unique<Field<bool>>(std::move(name), value);
        }
        static std::unique_ptr<FieldBase> double_field(std::string name, double value = {}) {
            return std::make_unique<Field<double>>(std::move(name), value);
        }
        static std::unique_ptr<FieldBase> float_field(std::string name, float value = {}) {
            return std::make_unique<Field<float>>(std::move(name), value);
        }
        static std::unique_ptr<FieldBase> int_field(std::string name, int32_t value = {}) {
            return std::make_unique<Field<int32_t>>(std::move(name), value);
        }
        static std::unique_ptr<FieldBase> ll_int_field(std::string name, int64_t value = {}) {
            return std::make_unique<Field<int64_t>>(std::move(name), value);
        }
        static std::unique_ptr<FieldBase> json_field(std::string name, Json::Value json_value = {}) {
            return std::make_unique<Field<Json::Value>>(std::move(name), std::move(json_value));
        }
        static std::unique_ptr<FieldBase> time_field(
            std::string name, std::chrono::system_clock::time_point time = {}) {
            return std::make_unique<Field<std::chrono::system_clock::time_point>>(std::move(name), time);
        }
    };
} // namespace demiplane::database
