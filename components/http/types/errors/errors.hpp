#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <http_enums.hpp>

namespace demiplane::http {

    struct Response;  // defined in Task 8

    struct BadRequestError {
        std::string message;
    };

    struct UnauthorizedError {
        std::string message;
    };

    struct ForbiddenError {
        std::string message;
    };

    struct NotFoundError {
        std::string resource;
        std::string id;
    };

    struct ConflictError {
        std::string message;
    };

    struct FieldError {
        std::string field;
        std::string detail;
    };

    struct UnprocessableEntityError {
        std::string message;
        std::vector<FieldError> fields;
    };

    struct PayloadTooLargeError {
        std::size_t limit = 0;
    };

    struct MethodNotAllowedError {
        std::vector<HttpMethod> allowed;
    };

    struct JsonParseError {
        std::string detail;
    };

    struct FormParseError {
        std::string detail;
    };

    struct MultipartParseError {
        std::string detail;
    };

    struct BodyLimitExceeded {
        std::size_t limit = 0;
    };

    // ── ADL conversions ──────────────────────────────────────────────────
    // Intentionally arena-free: error responses (4xx/5xx) are the cold path
    // and construct on the global heap via the static ResponseFactory (spec
    // §5.5/§5.7). Keeps the user's extension point a clean 1-arg free function.
    Response to_http_response(const BadRequestError& e);
    Response to_http_response(const UnauthorizedError& e);
    Response to_http_response(const ForbiddenError& e);
    Response to_http_response(const NotFoundError& e);
    Response to_http_response(const ConflictError& e);
    Response to_http_response(const UnprocessableEntityError& e);
    Response to_http_response(const PayloadTooLargeError& e);
    Response to_http_response(const MethodNotAllowedError& e);
    Response to_http_response(const JsonParseError& e);
    Response to_http_response(const FormParseError& e);
    Response to_http_response(const MultipartParseError& e);
    Response to_http_response(const BodyLimitExceeded& e);

}  // namespace demiplane::http
