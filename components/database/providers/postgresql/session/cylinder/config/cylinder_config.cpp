#include "cylinder_config.hpp"

namespace demiplane::db::postgres {

    [[nodiscard]] Json::Value CylinderConfig::wrapped_serialize() const {
        Json::Value result;
        result["capacity"]              = capacity_;
        result["min_connections"]       = min_connections_;
        result["connect_timeout"]       = connect_timeout_.count();
        result["idle_timeout"]          = idle_timeout_.count();
        result["health_check_interval"] = health_check_interval_.count();
        result["max_lifetime"]          = max_lifetime_.count();
        result["cleanup_sql"]           = cleanup_sql_;
        return result;
    }

    void CylinderConfig::wrapped_deserialize(const Json::Value& config) {
        capacity(config.get("capacity", 16U).asUInt64());
        min_connections(config.get("min_connections", 2U).asUInt64());
        connect_timeout(std::chrono::seconds{config.get("connect_timeout", 10).asInt64()});
        idle_timeout(std::chrono::seconds{config.get("idle_timeout", 300).asInt64()});
        health_check_interval(std::chrono::seconds{config.get("health_check_interval", 30).asInt64()});
        max_lifetime(std::chrono::seconds{config.get("max_lifetime", 3600).asInt64()});
        cleanup_sql(config.get("cleanup_sql", "DISCARD ALL").asString());
    }

}  // namespace demiplane::db::postgres
