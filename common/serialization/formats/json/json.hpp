#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include <json/json.h>
#include <serial_concepts.hpp>

namespace demiplane::serialization {

    // ---- write_field overloads for Json::Value ----

    void write_field(Json::Value& out, const std::string& key, const std::string& v);
    void write_field(Json::Value& out, const std::string& key, std::string_view v);
    void write_field(Json::Value& out, const std::string& key, int v);
    void write_field(Json::Value& out, const std::string& key, std::size_t v);
    void write_field(Json::Value& out, const std::string& key, bool v);
    void write_field(Json::Value& out, const std::string& key, double v);
    void write_field(Json::Value& out, const std::string& key, const std::filesystem::path& v);
    void write_field(Json::Value& out, const std::string& key, const std::map<std::string, std::string>& v);

    // Json::Value passthrough (for nested serialized objects)
    void write_field(Json::Value& out, const std::string& key, const Json::Value& v);

    template <typename Rep, typename Period>
    void write_field(Json::Value& out, const std::string& key, std::chrono::duration<Rep, Period> d) {
        out[std::string{key}] = static_cast<Json::Int64>(d.count());
    }

    template <typename E>
        requires std::is_enum_v<E>
    void write_field(Json::Value& out, const std::string& key, E v) {
        out[std::string{key}] = static_cast<int>(v);
    }

    template <typename T>
    void write_field(Json::Value& out, const std::string& key, const std::optional<T>& v) {
        if (v) {
            write_field(out, key, *v);
        }
    }

    template <typename T>
        requires HasFields<T>
    void write_field(Json::Value& out, const std::string& key, const T& nested) {
        out[std::string{key}] = nested.template serialize<Json::Value>();
    }

    // ---- read_field overloads for Json::Value ----
    // Return true if the field was present and read successfully.

    bool read_field(const Json::Value& in, const std::string& key, std::string& v);
    bool read_field(const Json::Value& in, const std::string& key, int& v);
    bool read_field(const Json::Value& in, const std::string& key, std::size_t& v);
    bool read_field(const Json::Value& in, const std::string& key, bool& v);
    bool read_field(const Json::Value& in, const std::string& key, double& v);
    bool read_field(const Json::Value& in, const std::string& key, std::filesystem::path& v);
    bool read_field(const Json::Value& in, const std::string& key, std::map<std::string, std::string>& v);
    bool read_field(const Json::Value& in, const std::string& key, Json::Value& v);

    template <typename Rep, typename Period>
    bool read_field(const Json::Value& in, const std::string& key, std::chrono::duration<Rep, Period>& d) {
        if (!in.isMember(key)) {
            return false;
        }
        d = std::chrono::duration<Rep, Period>{static_cast<Rep>(in[key].asInt64())};
        return true;
    }

    template <typename E>
        requires std::is_enum_v<E>
    bool read_field(const Json::Value& in, const std::string& key, E& v) {
        if (!in.isMember(key)) {
            return false;
        }
        v = static_cast<E>(in[key].asInt());
        return true;
    }

    template <typename T>
    bool read_field(const Json::Value& in, const std::string& key, std::optional<T>& v) {
        if (!in.isMember(key)) {
            return false;
        }
        if (T inner{}; read_field(in, key, inner)) {
            v = std::move(inner);
            return true;
        }
        return false;
    }

    template <typename T>
        requires HasFields<T>
    bool read_field(const Json::Value& in, const std::string& key, T& nested) {
        if (!in.isMember(key)) {
            return false;
        }
        nested = T::template deserialize<Json::Value>(in[key]);
        return true;
    }

}  // namespace demiplane::serialization
