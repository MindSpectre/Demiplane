#pragma once

#include <memory>
#include <memory_resource>
#include <span>
#include <string_view>

#include <body.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <connection_concepts.hpp>
#include <http_enums.hpp>
#include <request_context.hpp>
#include <response_factory.hpp>
#include <router.hpp>

#include "http11_config.hpp"

namespace demiplane::http {

    namespace detail {
        // Split allocators (PR3 D1, spike-validated): pmr BODY allocator (arena),
        // std::allocator FIELDS (so req.base() is http::fields, compatible with
        // the landed Headers::view_of_beast).
        using Http11Body =
            boost::beast::http::basic_string_body<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;
        using Http11Request = boost::beast::http::request<Http11Body>;
        using Http11Parser  = boost::beast::http::request_parser<Http11Body, std::allocator<char>>;

        /// Wrap a parsed Beast request as a RequestContext. The request body is
        /// a non-owning view over the parser's arena-backed bytes — the parser
        /// MUST outlive the context (the driver keeps it alive across dispatch +
        /// write, then resets the arena).
        RequestContext build_request_context(Http11Request& req, std::pmr::polymorphic_allocator<> arena);

        /// Translate our Response into a zero-copy buffer_body message whose
        /// body bytes point at `resp.body` — `resp` MUST outlive the returned
        /// message and its write.
        boost::beast::http::response<boost::beast::http::buffer_body> make_beast_response(Response& resp);

        bool is_malformed_request(const boost::beast::error_code& ec) noexcept;
    }  // namespace detail

    /**
     * @brief HTTP/1.1 driver over Boost.Beast (spec §6.3).
     *
     * One keep-alive session loop per connection: parse (body into the request
     * arena), build a RequestContext, dispatch through the Router, stamp
     * Date/Server, write the Response zero-copy. The bug-fix battery lands here
     * (body/header limits, per-phase timeouts, handler-exception → 500,
     * cancellation-aware I/O).
     */
    class Http11Driver {
    public:
        explicit Http11Driver(const Http11Config& cfg) noexcept
            : cfg_{cfg} {
        }

        [[nodiscard]] static constexpr Protocol id() noexcept {
            return Protocol::http1;
        }

        [[nodiscard]] static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view ALPNS[] = {"http/1.1"};
            return ALPNS;
        }

        template <StreamConnection ConnT>
        boost::asio::awaitable<void> serve(ConnT& conn, Router& router);

    private:
        static void stamp_common_headers(Response& resp);

        template <typename Stream>
        static boost::asio::awaitable<boost::beast::error_code>
        write_response(Stream& stream, Response& resp, boost::asio::cancellation_slot slot);

        Http11Config cfg_;
        // No SCROLL_COMPONENT_PREFIX / COMPONENT_LOG_* here: the h1 driver does
        // not log in v1, so it carries no Scroll dependency in its public header
        // (avoids a PUBLIC Scroll link just to satisfy the macro in consumers).
        // The h2/h3 scaffolds DO log, and link Scroll INTERFACE accordingly.
    };


    template <StreamConnection ConnT>
    boost::asio::awaitable<void> Http11Driver::serve(ConnT& conn, Router& router) {
        namespace asio = boost::asio;
        namespace http = boost::beast::http;

        auto& stream = conn.stream();

        // One buffer for the whole keep-alive session, NOT per request. A single
        // read can pull the next pipelined request's bytes in alongside the current
        // one; the parser consumes only its own message and leaves the remainder
        // here for the next iteration. Re-creating it each loop would silently drop
        // those already-read bytes (they are gone from the stream too) and re-alloc
        // every request — the same reuse rationale as the per-connection arena.
        boost::beast::flat_buffer buffer;

        while (true) {
            conn.reset_request_arena();
            std::pmr::polymorphic_allocator<char> body_alloc{conn.arena_alloc().resource()};

            detail::Http11Parser parser{
                std::piecewise_construct, std::forward_as_tuple(body_alloc), std::forward_as_tuple()};
            parser.header_limit(static_cast<std::uint32_t>(cfg_.max_header_bytes));
            parser.body_limit(cfg_.max_body_bytes);

            boost::beast::error_code ec;

            // Phase 1: header (also the idle wait between keep-alive requests)
            // TODO: cfg_.idle_timeout is currently unused — maybe wire it in here so
            //  a kept-alive connection waiting for its NEXT request uses idle_timeout,
            //  while header_timeout bounds only a header that is already mid-arrival.
            conn.expires_after(cfg_.header_timeout);
            co_await http::async_read_header(
                stream,
                buffer,
                parser,
                asio::bind_cancellation_slot(conn.cancel_slot(), asio::redirect_error(asio::use_awaitable, ec)));
            if (ec == http::error::end_of_stream)
                break;  // client closed cleanly
            if (ec == http::error::header_limit) {
                Response r   = ResponseFactory::bad_request("Request Header Fields Too Large");
                r.keep_alive = false;
                stamp_common_headers(r);
                co_await write_response(stream, r, conn.cancel_slot());
                break;
            }
            // Beast checks Content-Length against body_limit eagerly at header-parse
            // time; the error surfaces here, not in phase-2 async_read.
            if (ec == http::error::body_limit) {
                Response r   = ResponseFactory::payload_too_large();
                r.keep_alive = false;
                stamp_common_headers(r);
                co_await write_response(stream, r, conn.cancel_slot());
                break;
            }
            if (ec) {
                if (detail::is_malformed_request(ec)) {
                    Response r   = ResponseFactory::bad_request("Bad Request");
                    r.keep_alive = false;
                    stamp_common_headers(r);
                    co_await write_response(stream, r, conn.cancel_slot());
                }
                break;  // malformed → 400 (written above); transport error → just close
            }

            // Phase 2: body
            conn.expires_after(cfg_.body_timeout);
            co_await http::async_read(
                stream,
                buffer,
                parser,
                asio::bind_cancellation_slot(conn.cancel_slot(), asio::redirect_error(asio::use_awaitable, ec)));
            if (ec == http::error::body_limit) {
                Response r   = ResponseFactory::payload_too_large();
                r.keep_alive = false;
                stamp_common_headers(r);
                co_await write_response(stream, r, conn.cancel_slot());
                break;
            }
            if (ec) {
                if (detail::is_malformed_request(ec)) {
                    Response r   = ResponseFactory::bad_request("Bad Request");
                    r.keep_alive = false;
                    stamp_common_headers(r);
                    co_await write_response(stream, r, conn.cancel_slot());
                }
                break;
            }

            // Dispatch
            auto& req                    = parser.get();
            const bool client_keep_alive = req.keep_alive();
            RequestContext ctx           = detail::build_request_context(req, conn.arena_alloc());

            Response response{conn.arena_alloc()};
            try {
                response = co_await router.dispatch(std::move(ctx));
            } catch (...) {
                response            = ResponseFactory::internal_error();
                response.keep_alive = false;
            }

            const bool keep_alive = response.keep_alive && client_keep_alive;
            response.keep_alive   = keep_alive;
            response.version      = HttpVersion::http_1_1;
            stamp_common_headers(response);

            if (const boost::beast::error_code write_ec = co_await write_response(stream, response, conn.cancel_slot());
                write_ec || !keep_alive)
                break;
        }
        co_await conn.async_close();
    }

    template <typename Stream>
    boost::asio::awaitable<boost::beast::error_code>
    Http11Driver::write_response(Stream& stream, Response& resp, const boost::asio::cancellation_slot slot) {
        namespace asio = boost::asio;
        namespace http = boost::beast::http;

        auto msg = detail::make_beast_response(resp);
        boost::beast::error_code ec;
        co_await http::async_write(
            stream, msg, asio::bind_cancellation_slot(slot, asio::redirect_error(asio::use_awaitable, ec)));
        co_return ec;  // checked by serve()'s normal path; error paths break regardless
    }
}  // namespace demiplane::http
