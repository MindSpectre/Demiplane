#pragma once

#include <demiplane/gears>

#include <boost/asio/awaitable.hpp>

namespace demiplane::http {

    struct Response;  // defined in Task 8

    /// asio coroutine yielding a typed-error sum result. Handlers/middleware
    /// return AsyncOutcome<Response, Errors...>; the bind layer (PR2) collapses
    /// the held alternative into a plain Response via ADL to_http_response.
    template <typename T, typename... Es>
    using AsyncOutcome = boost::asio::awaitable<gears::Outcome<T, Es...>>;

    /// The common "no typed errors" case. Response is a struct (Task 8) — the
    /// forward declaration above suffices for the alias; users who instantiate
    /// it include <response/response.hpp>.
    using AsyncResponse = boost::asio::awaitable<Response>;

    using AsyncVoid = boost::asio::awaitable<void>;

}  // namespace demiplane::http
