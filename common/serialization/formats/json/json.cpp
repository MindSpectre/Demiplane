#include "json.hpp"

namespace demiplane::serialization {

    // ---- write_field implementations ----

    void write_field(Json::Value& out, const std::string& key, const std::string& v) {
        out[key] = v;
    }

    void write_field(Json::Value& out, const std::string& key, std::string_view v) {
        out[key] = std::string{v};
    }

    void write_field(Json::Value& out, const std::string& key, int v) {
        out[key] = v;
    }

    void write_field(Json::Value& out, const std::string& key, std::size_t v) {
        out[key] = v;
    }

    void write_field(Json::Value& out, const std::string& key, bool v) {
        out[key] = v;
    }

    void write_field(Json::Value& out, const std::string& key, double v) {
        out[key] = v;
    }

    void write_field(Json::Value& out, const std::string& key, const std::filesystem::path& v) {
        out[key] = v.string();
    }

    void write_field(Json::Value& out, const std::string& key, const std::map<std::string, std::string>& v) {
        Json::Value obj{Json::objectValue};
        for (const auto& [mk, mv] : v) {
            obj[mk] = mv;
        }
        out[key] = std::move(obj);
    }

    void write_field(Json::Value& out, const std::string& key, const Json::Value& v) {
        out[key] = v;
    }

    // ---- read_field implementations ----

    bool read_field(const Json::Value& in, const std::string& key, std::string& v) {
        if (!in.isMember(key)) {
            return false;
        }
        v = in[key].asString();
        return true;
    }

    bool read_field(const Json::Value& in, const std::string& key, int& v) {
        if (!in.isMember(key)) {
            return false;
        }
        v = in[key].asInt();
        return true;
    }

    bool read_field(const Json::Value& in, const std::string& key, std::size_t& v) {
        if (!in.isMember(key)) {
            return false;
        }
        v = in[key].asUInt64();
        return true;
    }

    bool read_field(const Json::Value& in, const std::string& key, bool& v) {
        if (!in.isMember(key)) {
            return false;
        }
        v = in[key].asBool();
        return true;
    }

    bool read_field(const Json::Value& in, const std::string& key, double& v) {
        if (!in.isMember(key)) {
            return false;
        }
        v = in[key].asDouble();
        return true;
    }

    bool read_field(const Json::Value& in, const std::string& key, std::filesystem::path& v) {
        if (!in.isMember(key)) {
            return false;
        }
        v = in[key].asString();
        return true;
    }

    bool read_field(const Json::Value& in, const std::string& key, std::map<std::string, std::string>& v) {
        if (!in.isMember(key)) {
            return false;
        }
        for (const auto& obj = in[key]; const auto& mk : obj.getMemberNames()) {
            v[mk] = obj[mk].asString();
        }
        return true;
    }

    bool read_field(const Json::Value& in, const std::string& key, Json::Value& v) {
        if (!in.isMember(key)) {
            return false;
        }
        v = in[key];
        return true;
    }

}  // namespace demiplane::serialization
