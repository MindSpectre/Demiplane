#include "response_factory.hpp"

#include <boost/beast/http.hpp>

namespace demiplane::http {

    Response ResponseFactory::ok(std::string body, const std::string_view content_type, const std::uint32_t version) {
        Response res{boost::beast::http::status::ok, version};
        res.set(boost::beast::http::field::content_type, content_type);
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.body() = std::move(body);
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::json(std::string body, const std::uint32_t version) {
        return ok(std::move(body), "application/json", version);
    }

    Response
    ResponseFactory::created(std::string body, const std::string_view content_type, const std::uint32_t version) {
        Response res{boost::beast::http::status::created, version};
        res.set(boost::beast::http::field::content_type, content_type);
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.body() = std::move(body);
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::not_found(std::string message, const std::uint32_t version) {
        Response res{boost::beast::http::status::not_found, version};
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.body() = std::move(message);
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::bad_request(std::string message, const std::uint32_t version) {
        Response res{boost::beast::http::status::bad_request, version};
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.body() = std::move(message);
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::internal_error(std::string message, const std::uint32_t version) {
        Response res{boost::beast::http::status::internal_server_error, version};
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.body() = std::move(message);
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::redirect(const std::string_view location, const std::uint32_t version) {
        Response res{boost::beast::http::status::found, version};
        res.set(boost::beast::http::field::location, location);
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::no_content(const std::uint32_t version) {
        Response res{boost::beast::http::status::no_content, version};
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::unauthorized(std::string message, const std::uint32_t version) {
        Response res{boost::beast::http::status::unauthorized, version};
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.body() = std::move(message);
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::forbidden(std::string message, const std::uint32_t version) {
        Response res{boost::beast::http::status::forbidden, version};
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.body() = std::move(message);
        res.prepare_payload();
        return res;
    }

    Response ResponseFactory::custom(const boost::beast::http::status status,
                                     std::string body,
                                     const std::string_view content_type,
                                     const std::uint32_t version) {
        Response res{status, version};
        res.set(boost::beast::http::field::content_type, content_type);
        res.set(boost::beast::http::field::server, "demiplane/http");
        res.body() = std::move(body);
        res.prepare_payload();
        return res;
    }

}  // namespace demiplane::http
