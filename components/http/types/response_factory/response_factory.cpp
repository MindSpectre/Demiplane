#include "response_factory.hpp"

#include <utility>

namespace demiplane::http {

    namespace {
        Response with_body(HttpStatus s, std::string body, std::string_view ct) {
            Response r;                               // default alloc = new_delete (cold path)
            r.status = s;
            r.add_header("Content-Type", ct);
            r.body = Body::owned(std::move(body));
            return r;
        }
    }

    Response ResponseFactory::ok(std::string b, std::string_view ct)      { return with_body(HttpStatus::ok, std::move(b), ct); }
    Response ResponseFactory::json(std::string b)                         { return with_body(HttpStatus::ok, std::move(b), "application/json"); }
    Response ResponseFactory::created(std::string b, std::string_view ct) { return with_body(HttpStatus::created, std::move(b), ct); }

    Response ResponseFactory::no_content() {
        Response r; r.status = HttpStatus::no_content; return r;   // EmptyBody, no Content-Type
    }
    Response ResponseFactory::redirect(std::string_view location, HttpStatus status) {
        Response r; r.status = status; r.add_header("Location", location); return r;
    }

    Response ResponseFactory::not_found(std::string b)            { return with_body(HttpStatus::not_found, std::move(b), "text/plain"); }
    Response ResponseFactory::bad_request(std::string b)          { return with_body(HttpStatus::bad_request, std::move(b), "text/plain"); }
    Response ResponseFactory::unauthorized(std::string b)         { return with_body(HttpStatus::unauthorized, std::move(b), "text/plain"); }
    Response ResponseFactory::forbidden(std::string b)            { return with_body(HttpStatus::forbidden, std::move(b), "text/plain"); }
    Response ResponseFactory::conflict(std::string b)             { return with_body(HttpStatus::conflict, std::move(b), "text/plain"); }
    Response ResponseFactory::payload_too_large(std::string b)    { return with_body(HttpStatus::payload_too_large, std::move(b), "text/plain"); }
    Response ResponseFactory::unprocessable_entity(std::string b) { return with_body(HttpStatus::unprocessable_entity, std::move(b), "text/plain"); }
    Response ResponseFactory::internal_error(std::string b)       { return with_body(HttpStatus::internal_server_error, std::move(b), "text/plain"); }

    Response ResponseFactory::method_not_allowed(std::span<const HttpMethod> allow) {
        Response r; r.status = HttpStatus::method_not_allowed;
        std::string v; bool first = true;
        for (auto m : allow) { if (!first) v += ", "; v += to_string(m); first = false; }
        r.add_header("Allow", v);
        return r;
    }
    Response ResponseFactory::custom(HttpStatus s, std::string b, std::string_view ct) {
        return with_body(s, std::move(b), ct);
    }

}  // namespace demiplane::http
