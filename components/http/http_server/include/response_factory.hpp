#pragma once

#pragma once

#include <string>

#include "aliases.hpp"

namespace demiplane::http {

    class ResponseFactory {
    public:
        static Response ok(
            std::string body = "", std::string_view content_type = "text/plain", std::uint32_t version = 11);
        static Response json(std::string body, std::uint32_t version = 11);
        static Response created(
            std::string body = "", std::string_view content_type = "application/json", std::uint32_t version = 11);
        static Response not_found(std::string message = "Not Found", std::uint32_t version = 11);
        static Response bad_request(std::string message = "Bad Request", std::uint32_t version = 11);
        static Response internal_error(std::string message = "Internal Server Error", std::uint32_t version = 11);
        static Response redirect(std::string_view location, std::uint32_t version = 11);
        static Response no_content(std::uint32_t version = 11);
        static Response unauthorized(std::string message = "Unauthorized", std::uint32_t version = 11);
        static Response forbidden(std::string message = "Forbidden", std::uint32_t version = 11);

        // Custom response builder
        static Response custom(boost::beast::http::status status, std::string body,
            std::string_view content_type = "text/plain", std::uint32_t version = 11);
    };

} // namespace demiplane::http
