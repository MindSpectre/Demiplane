#include "response_factory.hpp"

#include <utility>

namespace demiplane::http {

    namespace {
        Response with_body(const HttpStatus s, std::string body, const std::string_view ct) {
            Response r;  // default alloc = new_delete (cold path)
            r.status = s;
            r.add_header("Content-Type", ct);
            r.body = Body::owned(std::move(body));
            return r;
        }
    }  // namespace

    Response ResponseFactory::ok(std::string body, const std::string_view ct) {
        return with_body(HttpStatus::ok, std::move(body), ct);
    }
    Response ResponseFactory::json(std::string body) {
        return with_body(HttpStatus::ok, std::move(body), "application/json");
    }
    Response ResponseFactory::created(std::string body, const std::string_view ct) {
        return with_body(HttpStatus::created, std::move(body), ct);
    }

    Response ResponseFactory::no_content() {
        Response r;
        r.status = HttpStatus::no_content;
        return r;  // EmptyBody, no Content-Type
    }
    Response ResponseFactory::redirect(const std::string_view location, const HttpStatus status) {
        Response r;
        r.status = status;
        r.add_header("Location", location);
        return r;
    }

    Response ResponseFactory::not_found(std::string body) {
        return with_body(HttpStatus::not_found, std::move(body), "text/plain");
    }
    Response ResponseFactory::bad_request(std::string body) {
        return with_body(HttpStatus::bad_request, std::move(body), "text/plain");
    }
    Response ResponseFactory::unauthorized(std::string body) {
        return with_body(HttpStatus::unauthorized, std::move(body), "text/plain");
    }
    Response ResponseFactory::forbidden(std::string body) {
        return with_body(HttpStatus::forbidden, std::move(body), "text/plain");
    }
    Response ResponseFactory::conflict(std::string body) {
        return with_body(HttpStatus::conflict, std::move(body), "text/plain");
    }
    Response ResponseFactory::payload_too_large(std::string body) {
        return with_body(HttpStatus::payload_too_large, std::move(body), "text/plain");
    }
    Response ResponseFactory::unprocessable_entity(std::string body) {
        return with_body(HttpStatus::unprocessable_entity, std::move(body), "text/plain");
    }
    Response ResponseFactory::internal_error(std::string body) {
        return with_body(HttpStatus::internal_server_error, std::move(body), "text/plain");
    }

    Response ResponseFactory::method_not_allowed(const std::span<const HttpMethod> allow) {
        Response r;
        r.status = HttpStatus::method_not_allowed;
        std::string v;
        bool first = true;
        for (const auto m : allow) {
            if (!first)
                v += ", ";
            v     += to_string(m);
            first  = false;
        }
        r.add_header("Allow", v);
        return r;
    }
    Response ResponseFactory::custom(const HttpStatus status, std::string body, const std::string_view ct) {
        return with_body(status, std::move(body), ct);
    }

}  // namespace demiplane::http
