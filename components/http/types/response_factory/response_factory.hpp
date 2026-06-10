#pragma once

#include <span>
#include <string>
#include <string_view>

#include <http_enums.hpp>
#include <response.hpp>

namespace demiplane::http {

    /// Static, GLOBAL-HEAP, cold-path factory: error conversions
    /// (to_http_response) and library/test/synthetic responses. The hot path is
    /// RequestContext's arena-bound factories (ctx.json/ok/...). Neither sets
    /// Date/Server (drivers stamp those).
    class ResponseFactory {
    public:
        static Response ok(std::string body = "", std::string_view ct = "text/plain");
        static Response json(std::string body);
        static Response created(std::string body = "", std::string_view ct = "application/json");
        static Response no_content();
        static Response redirect(std::string_view location, HttpStatus status = HttpStatus::found);
        static Response not_found(std::string body = "Not Found");
        static Response bad_request(std::string body = "Bad Request");
        static Response unauthorized(std::string body = "Unauthorized");
        static Response forbidden(std::string body = "Forbidden");
        static Response conflict(std::string body = "Conflict");
        static Response payload_too_large(std::string body = "Payload Too Large");
        static Response unprocessable_entity(std::string body = "Unprocessable Entity");
        static Response internal_error(std::string body = "Internal Server Error");
        static Response method_not_allowed(std::span<const HttpMethod> allow);
        static Response custom(HttpStatus status, std::string body, std::string_view ct = "text/plain");
    };

}  // namespace demiplane::http
