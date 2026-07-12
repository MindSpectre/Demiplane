#pragma once

#include <demiplane/gears>

#include <boost/asio/awaitable.hpp>
#include <executor.hpp>

namespace demiplane::http {

    struct Response;  // defined in Task 8

    /// asio coroutine yielding a typed-error sum result. Handlers/middleware
    /// return AsyncOutcome<Response, Errors...>; the bind layer (PR2) collapses
    /// the held alternative into a plain Response via ADL to_http_response.
    ///
    /// Typed on the connection Strand, NOT the default any_io_executor:
    /// handlers always run on the connection's strand, and an erased frame
    /// re-introduces virtual executor dispatch on every await in the request
    /// loop (README Finding 12). Ops co_awaited inside these coroutines must
    /// use the matching `use_strand_awaitable` token (executor.hpp).
    template <typename T, typename... Es>
    using AsyncOutcome = boost::asio::awaitable<gears::Outcome<T, Es...>, Strand>;

    /// The common "no typed errors" case. Response is a struct (Task 8) — the
    /// forward declaration above suffices for the alias; users who instantiate
    /// it include <response/response.hpp>.
    using AsyncResponse = boost::asio::awaitable<Response, Strand>;

    using AsyncVoid = boost::asio::awaitable<void, Strand>;

}  // namespace demiplane::http
