#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
namespace demiplane::http {
    class RequestContext;
    using Request        = boost::beast::http::request<boost::beast::http::string_body>;
    using Response       = boost::beast::http::response<boost::beast::http::string_body>;
    using AsyncResponse  = boost::asio::awaitable<Response>;
    using AsyncVoid      = boost::asio::awaitable<void>;
    using Handler        = std::function<AsyncResponse(Request)>;
    using ContextHandler = std::function<AsyncResponse(RequestContext)>;
    using Middleware =
        std::function<boost::asio::awaitable<void>(Request&, Response&, std::function<boost::asio::awaitable<void>()>)>;

    // Callback types for server events
    using ServerCallback        = std::function<void()>;
    using AsyncServerCallback   = std::function<AsyncVoid()>;
    using RequestCallback       = std::function<void(const Request&)>;
    using AsyncRequestCallback  = std::function<AsyncVoid(const Request&)>;
    using ResponseCallback      = std::function<void(const Response&)>;
    using AsyncResponseCallback = std::function<AsyncVoid(const Response&)>;
    using ErrorCallback         = std::function<void(const std::exception&)>;
    using AsyncErrorCallback    = std::function<AsyncVoid(const std::exception&)>;

} // namespace demiplane::http