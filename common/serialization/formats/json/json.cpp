#include "json.hpp"

namespace demiplane::serialization {

    // ---- write_field implementations ----

    void write_field(Json::Value& out, const FieldName key, const std::string& v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const std::string_view v) {
        out[key.str()] = std::string{v};
    }

    void write_field(Json::Value& out, const FieldName key, const int v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const std::size_t v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const std::uint16_t v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const bool v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const double v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const std::filesystem::path& v) {
        out[key.str()] = v.string();
    }

    void write_field(Json::Value& out, const FieldName key, const std::map<std::string, std::string>& v) {
        Json::Value obj{Json::objectValue};
        for (const auto& [mk, mv] : v) {
            obj[mk] = mv;
        }
        out[key.str()] = std::move(obj);
    }

    void write_field(Json::Value& out, const FieldName key, const Json::Value& v) {
        out[key.str()] = v;
    }

    // ---- read_field implementations ----

    bool read_field(const Json::Value& in, const FieldName key, std::string& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asString();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, int& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asInt();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, std::size_t& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asUInt64();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, std::uint16_t& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const auto raw = in[k].asUInt();  // Json::LogicError on type mismatch / negative
        if (raw > 0xFFFF) {
            throw std::invalid_argument{"config field '" + k + "': value " + std::to_string(raw) +
                                        " exceeds 65535"};
        }
        v = static_cast<std::uint16_t>(raw);
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, bool& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asBool();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, double& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asDouble();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, std::filesystem::path& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asString();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, std::map<std::string, std::string>& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v.clear();
        for (const auto& obj = in[k]; const auto& mk : obj.getMemberNames()) {
            v[mk] = obj[mk].asString();
        }
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, Json::Value& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k];
        return true;
    }

}  // namespace demiplane::serialization
