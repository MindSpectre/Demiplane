#include "postgres_connection_config.hpp"

#include <utility>

namespace demiplane::db::postgres {

    [[nodiscard]] Json::Value ConnectionConfig::wrapped_serialize() const {
        Json::Value result = credentials_.serialize();

        // Node configuration
        result["role"]         = static_cast<int>(role_);
        result["priority"]     = priority_;
        result["cluster_name"] = cluster_name_;

        // Timeouts (as seconds)
        result["connect_timeout"]             = static_cast<Json::Int64>(connect_timeout_.count());
        result["statement_timeout"]           = static_cast<Json::Int64>(statement_timeout_.count());
        result["idle_in_transaction_timeout"] = static_cast<Json::Int64>(idle_in_transaction_timeout_.count());
        result["lock_timeout"]                = static_cast<Json::Int64>(lock_timeout_.count());

        // SSL
        result["ssl_mode"] = static_cast<int>(ssl_mode_);
        if (ssl_cert_) {
            result["ssl_cert"] = *ssl_cert_;
        }
        if (ssl_key_) {
            result["ssl_key"] = *ssl_key_;
        }
        if (ssl_root_cert_) {
            result["ssl_root_cert"] = *ssl_root_cert_;
        }

        // Protocol
        result["binary_protocol"]  = binary_protocol_;
        result["auto_prepare"]     = auto_prepare_;
        result["pipeline_mode"]    = pipeline_mode_;
        result["application_name"] = application_name_;
        result["search_path"]      = search_path_;

        // Performance
        result["work_mem_mb"] = work_mem_mb_;
        result["jit"]         = jit_;

        // Extra options
        Json::Value extras{Json::objectValue};
        for (const auto& [key, value] : extra_options_) {
            extras[key] = value;
        }
        result["extra_options"] = extras;

        return result;
    }

    void ConnectionConfig::wrapped_deserialize(const Json::Value& config) {
        // Node configuration
        if (config.isMember("role")) {
            role(node_role_from_int(config["role"].asInt()));
        }
        if (config.isMember("priority")) {
            priority(config["priority"].asInt());
        }
        if (config.isMember("cluster_name")) {
            cluster_name(config["cluster_name"].asString());
        }

        // Timeouts
        if (config.isMember("connect_timeout")) {
            connect_timeout(std::chrono::seconds{config["connect_timeout"].asInt64()});
        }
        if (config.isMember("statement_timeout")) {
            statement_timeout(std::chrono::seconds{config["statement_timeout"].asInt64()});
        }
        if (config.isMember("idle_in_transaction_timeout")) {
            idle_in_transaction_timeout(std::chrono::seconds{config["idle_in_transaction_timeout"].asInt64()});
        }
        if (config.isMember("lock_timeout")) {
            lock_timeout(std::chrono::seconds{config["lock_timeout"].asInt64()});
        }

        // SSL
        if (config.isMember("ssl_mode")) {
            ssl_mode(ssl_mode_from_int(config["ssl_mode"].asInt()));
        }
        if (config.isMember("ssl_cert")) {
            ssl_cert(config["ssl_cert"].asString());
        }
        if (config.isMember("ssl_key")) {
            ssl_key(config["ssl_key"].asString());
        }
        if (config.isMember("ssl_root_cert")) {
            ssl_root_cert(config["ssl_root_cert"].asString());
        }

        // Protocol
        if (config.isMember("binary_protocol")) {
            binary_protocol(config["binary_protocol"].asBool());
        }
        if (config.isMember("auto_prepare")) {
            auto_prepare(config["auto_prepare"].asBool());
        }
        if (config.isMember("pipeline_mode")) {
            pipeline_mode(config["pipeline_mode"].asBool());
        }
        if (config.isMember("application_name")) {
            application_name(config["application_name"].asString());
        }
        if (config.isMember("search_path")) {
            search_path(config["search_path"].asString());
        }

        // Performance
        if (config.isMember("work_mem_mb")) {
            work_mem_mb(config["work_mem_mb"].asUInt64());
        }
        if (config.isMember("jit")) {
            jit(config["jit"].asBool());
        }

        // Extra options
        if (config.isMember("extra_options")) {
            for (const auto& extras = config["extra_options"]; const auto& key : extras.getMemberNames()) {
                extra_option(key, extras[key].asString());
            }
        }
    }
}  // namespace demiplane::db::postgres
