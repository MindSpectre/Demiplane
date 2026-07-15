#include "load_server_config.hpp"

#include <cerrno>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#include <response.hpp>
#include <response_factory.hpp>

namespace demiplane::http {

    namespace {
        /// jsoncpp formats parse failures as "* Line 3, Column 5\n  ...".
        std::size_t parse_error_line(const std::string& errs) noexcept {
            const auto pos = errs.find("Line ");
            if (pos == std::string::npos) {
                return 0;
            }
            std::size_t line     = 0;
            const char* first    = errs.data() + pos + 5;
            const char* last     = errs.data() + errs.size();
            const auto [ptr, ec] = std::from_chars(first, last, line);
            return ec == std::errc{} ? line : 0;
        }
    }  // namespace

    gears::Outcome<ServerConfig, ConfigFileError, ConfigParseError, ConfigSchemaError>
    load_server_config(const std::string_view path) {
        std::string path_str{path};

        if (std::error_code fs_ec; !std::filesystem::is_regular_file(path_str, fs_ec)) {
            return gears::err(ConfigFileError{std::move(path_str), fs_ec ? fs_ec.message() : "not a regular file"});
        }

        std::ifstream file{path_str, std::ios::binary};
        if (!file.is_open()) {
            const std::error_code open_ec{errno, std::generic_category()};
            return gears::err(ConfigFileError{std::move(path_str), open_ec.message()});
        }

        Json::Value root;
        std::string errs;
        if (Json::CharReaderBuilder reader; !Json::parseFromStream(reader, file, &root, &errs)) {
            const std::size_t line = parse_error_line(errs);
            return gears::err(ConfigParseError{std::move(path_str), line, std::move(errs)});
        }
        if (!root.isObject()) {
            return gears::err(ConfigSchemaError{std::move(path_str), "", "top-level JSON value must be an object"});
        }

        try {
            return ServerConfig::deserialize<Json::Value>(root);
        } catch (const Json::Exception& e) {  // asString()/asInt64()/... type mismatch
            return gears::err(ConfigSchemaError{std::move(path_str), "", std::string{"type mismatch: "} + e.what()});
        } catch (const std::invalid_argument& e) {  // enum codec / validate()
            return gears::err(ConfigSchemaError{std::move(path_str), "", e.what()});
        }
    }

    Json::Value dump_server_config(const ServerConfig& cfg) {
        return cfg.serialize<Json::Value>();
    }

    Response to_http_response(const ConfigFileError& e) {
        return ResponseFactory::internal_error("Config file error: " + e.path + ": " + e.reason);
    }

    Response to_http_response(const ConfigParseError& e) {
        return ResponseFactory::internal_error("Config parse error: " + e.path + " line " + std::to_string(e.line) +
                                               ": " + e.detail);
    }

    Response to_http_response(const ConfigSchemaError& e) {
        std::string body = "Config schema error: " + e.path;
        if (!e.field_path.empty()) {
            body += " field ";
            body += e.field_path;
        }
        body += ": ";
        body += e.detail;
        return ResponseFactory::internal_error(std::move(body));
    }

}  // namespace demiplane::http
