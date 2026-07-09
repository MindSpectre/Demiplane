#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <json/json.h>

#include <field.hpp>
#include <serial_concepts.hpp>

namespace demiplane::serialization {

    // ---- write_field overloads for Json::Value ----
    // The FieldName key parameter is the extension point's ADL anchor (see
    // field.hpp). Custom codecs (e.g. the HTTP enum-as-string overloads) must
    // use the same shape.

    void write_field(Json::Value& out, FieldName key, const std::string& v);
    void write_field(Json::Value& out, FieldName key, std::string_view v);
    void write_field(Json::Value& out, FieldName key, int v);
    void write_field(Json::Value& out, FieldName key, std::size_t v);
    void write_field(Json::Value& out, FieldName key, std::uint16_t v);
    void write_field(Json::Value& out, FieldName key, bool v);
    void write_field(Json::Value& out, FieldName key, double v);
    void write_field(Json::Value& out, FieldName key, const std::filesystem::path& v);
    void write_field(Json::Value& out, FieldName key, const std::map<std::string, std::string>& v);

    // Json::Value passthrough (for nested serialized objects)
    void write_field(Json::Value& out, FieldName key, const Json::Value& v);

    template <typename Rep, typename Period>
    void write_field(Json::Value& out, const FieldName key, std::chrono::duration<Rep, Period> d) {
        out[key.str()] = static_cast<Json::Int64>(d.count());
    }

    template <typename E>
        requires std::is_enum_v<E>
    void write_field(Json::Value& out, const FieldName key, E v) {
        out[key.str()] = static_cast<int>(v);
    }

    template <typename T>
    void write_field(Json::Value& out, const FieldName key, const std::optional<T>& v) {
        if (v) {
            write_field(out, key, *v);
        }
    }

    template <typename T>
        requires HasFields<T>
    void write_field(Json::Value& out, const FieldName key, const T& nested) {
        out[key.str()] = nested.template serialize<Json::Value>();
    }

    /// Vector fields encode as a JSON array. HasFields elements nest as
    /// objects; every other element type reuses its scalar/enum overload via a
    /// single-key wrap object (so ADL extension points work element-wise too).
    template <typename T>
    void write_field(Json::Value& out, const FieldName key, const std::vector<T>& v) {
        Json::Value arr{Json::arrayValue};
        for (const auto& element : v) {
            if constexpr (HasFields<T>) {
                arr.append(element.template serialize<Json::Value>());
            } else {
                Json::Value wrap{Json::objectValue};
                write_field(wrap, FieldName{"v"}, element);
                arr.append(wrap["v"]);
            }
        }
        out[key.str()] = std::move(arr);
    }

    // ---- read_field overloads for Json::Value ----
    // Return true if the field was present and read successfully. Convertible-
    // type mismatches throw Json::LogicError (jsoncpp's as*()); structural
    // mismatches (non-array for a vector) and range violations throw
    // std::invalid_argument.

    bool read_field(const Json::Value& in, FieldName key, std::string& v);
    bool read_field(const Json::Value& in, FieldName key, int& v);
    bool read_field(const Json::Value& in, FieldName key, std::size_t& v);
    bool read_field(const Json::Value& in, FieldName key, std::uint16_t& v);
    bool read_field(const Json::Value& in, FieldName key, bool& v);
    bool read_field(const Json::Value& in, FieldName key, double& v);
    bool read_field(const Json::Value& in, FieldName key, std::filesystem::path& v);
    bool read_field(const Json::Value& in, FieldName key, std::map<std::string, std::string>& v);
    bool read_field(const Json::Value& in, FieldName key, Json::Value& v);

    template <typename Rep, typename Period>
    bool read_field(const Json::Value& in, const FieldName key, std::chrono::duration<Rep, Period>& d) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        d = std::chrono::duration<Rep, Period>{static_cast<Rep>(in[k].asInt64())};
        return true;
    }

    template <typename E>
        requires std::is_enum_v<E>
    bool read_field(const Json::Value& in, const FieldName key, E& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = static_cast<E>(in[k].asInt());
        return true;
    }

    template <typename T>
    bool read_field(const Json::Value& in, const FieldName key, std::optional<T>& v) {
        if (!in.isMember(key.str())) {
            return false;
        }
        if constexpr (HasFields<T>) {
            // Construct through the framework path — T's default ctor is
            // typically private (Builder-only construction).
            v = T::template deserialize<Json::Value>(in[key.str()]);
            return true;
        } else {
            if (T inner{}; read_field(in, key, inner)) {
                v = std::move(inner);
                return true;
            }
            return false;
        }
    }

    template <typename T>
        requires HasFields<T>
    bool read_field(const Json::Value& in, const FieldName key, T& nested) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        nested = T::template deserialize<Json::Value>(in[k]);
        return true;
    }

    template <typename T>
    bool read_field(const Json::Value& in, const FieldName key, std::vector<T>& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const Json::Value& arr = in[k];
        if (!arr.isArray()) {
            throw std::invalid_argument{"config field '" + k + "': expected a JSON array"};
        }
        std::vector<T> result;
        result.reserve(arr.size());
        for (const auto& item : arr) {
            if constexpr (HasFields<T>) {
                result.push_back(T::template deserialize<Json::Value>(item));
            } else {
                Json::Value wrap{Json::objectValue};
                wrap["v"] = item;
                T element{};
                if (read_field(wrap, FieldName{"v"}, element)) {
                    result.push_back(std::move(element));
                }
            }
        }
        v = std::move(result);
        return true;
    }

}  // namespace demiplane::serialization
