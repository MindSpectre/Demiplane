#include "errors.hpp"

#include <sstream>

#include <response.hpp>
#include <response_factory.hpp>

namespace demiplane::http {

    Response to_http_response(const BadRequestError& e) {
        return ResponseFactory::bad_request(e.message);
    }

    Response to_http_response(const UnauthorizedError& e) {
        return ResponseFactory::unauthorized(e.message);
    }

    Response to_http_response(const ForbiddenError& e) {
        return ResponseFactory::forbidden(e.message);
    }

    Response to_http_response(const NotFoundError& e) {
        std::string body = e.resource;
        if (!e.id.empty()) {
            body += ' ';
            body += e.id;
        }
        body += " not found";
        return ResponseFactory::not_found(std::move(body));
    }

    Response to_http_response(const ConflictError& e) {
        return ResponseFactory::conflict(e.message);
    }

    Response to_http_response(const UnprocessableEntityError& e) {
        std::ostringstream os;
        os << e.message;
        for (const auto& [field, detail] : e.fields)
            os << "\n" << field << ": " << detail;
        return ResponseFactory::unprocessable_entity(os.str());
    }

    Response to_http_response(const PayloadTooLargeError& e) {
        std::ostringstream os;
        os << "Payload Too Large (limit " << e.limit << " bytes)";
        return ResponseFactory::payload_too_large(os.str());
    }

    Response to_http_response(const MethodNotAllowedError& e) {
        return ResponseFactory::method_not_allowed(e.allowed);
    }

    Response to_http_response(const JsonParseError& e) {
        return ResponseFactory::bad_request("JSON parse error: " + e.detail);
    }

    Response to_http_response(const FormParseError& e) {
        return ResponseFactory::bad_request("Form parse error: " + e.detail);
    }

    Response to_http_response(const MultipartParseError& e) {
        return ResponseFactory::bad_request("Multipart parse error: " + e.detail);
    }

    Response to_http_response(const BodyLimitExceeded& e) {
        std::ostringstream os;
        os << "Body Limit Exceeded (" << e.limit << " bytes)";
        return ResponseFactory::payload_too_large(os.str());
    }

}  // namespace demiplane::http
