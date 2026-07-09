#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include <demiplane/gears>
#include <json/json.h>

#include <server_config.hpp>

namespace demiplane::http {

    struct Response;  // conversions return it; defined in types/response

    /// Filesystem-level failure: missing / unreadable / not a regular file.
    struct ConfigFileError {
        std::string path;
        std::string reason;
    };

    /// Malformed JSON. `line` is best-effort (0 when it cannot be extracted
    /// from the reader's message); `detail` carries jsoncpp's full message.
    struct ConfigParseError {
        std::string path;
        std::size_t line = 0;
        std::string detail;
    };

    /// Well-formed JSON that does not describe a valid ServerConfig: a type
    /// mismatch, an unknown enum string, or a validate() failure. field_path
    /// is best-effort (often empty — `detail` names the offending field; full
    /// /listeners/1/tls/cert_file pointers would need path threading through
    /// every read_field overload, deferred — plan D4).
    struct ConfigSchemaError {
        std::string path;
        std::string field_path;
        std::string detail;
    };

    /**
     * @brief Load + validate a ServerConfig from a JSON file (spec §10.2).
     *
     * 1. Open the file (must be a regular file)   → ConfigFileError
     * 2. Parse JSON (Json::CharReader pipeline)   → ConfigParseError (+line)
     * 3. Deserialize via the fields() machinery   → ConfigSchemaError
     *    (type mismatch / unknown enum string)
     * 4. validate() (run by Builder::finalize)    → ConfigSchemaError
     *
     * Unknown JSON keys are ignored (the fields() walk reads known names
     * only). An empty file is a parse error, not an empty config.
     */
    gears::Outcome<ServerConfig, ConfigFileError, ConfigParseError, ConfigSchemaError> load_server_config(
        std::string_view path);

    /// Round-trip companion (spec §10.2): serialize — which validates first.
    /// Secret fields (tls.key_passphrase) are omitted by policy (plan D3).
    Json::Value dump_server_config(const ServerConfig& cfg);

    // ADL conversions (spec §10.2): config errors surfaced on admin endpoints.
    // Cold path — global heap via the static ResponseFactory (errors.cpp
    // convention); all three render as 500.
    Response to_http_response(const ConfigFileError& e);
    Response to_http_response(const ConfigParseError& e);
    Response to_http_response(const ConfigSchemaError& e);

}  // namespace demiplane::http
