#pragma once

#include <charconv>
#include <memory>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>

#include <body.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
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
        // BOTH allocators are the request arena. The fields allocator used to be
        // std::allocator (so req.base() was a plain http::fields for the landed
        // Headers::view_of_beast); that cost ~4 global-heap malloc/free per
        // request — one Beast node per header line plus the target. Headers now
        // views the pmr fields type directly (see BeastFields in headers.hpp).
        using Http11Body =
            boost::beast::http::basic_string_body<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;
        using Http11Request = boost::beast::http::request<Http11Body, BeastFields>;
        using Http11Parser  = boost::beast::http::request_parser<Http11Body, std::pmr::polymorphic_allocator<char>>;

        /// Response headers serialized out of the request arena. Beast allocates
        /// one `basic_fields` node per header; with the default std::allocator
        /// that is a global-heap malloc/free per header per response, ~3.2 per
        /// request measured — and the headers are ALREADY in the arena, so those
        /// nodes were pure copy-out waste.
        using ArenaFields    = boost::beast::http::basic_fields<std::pmr::polymorphic_allocator<char>>;
        using Http11Response = boost::beast::http::response<boost::beast::http::buffer_body, ArenaFields>;

        /// Wrap a parsed Beast request as a RequestContext. The request body is
        /// a non-owning view over the parser's arena-backed bytes — the parser
        /// MUST outlive the context (the driver keeps it alive across dispatch +
        /// write, then resets the arena).
        RequestContext build_request_context(Http11Request& req, std::pmr::polymorphic_allocator<> arena);

        /// Translate our Response into a zero-copy buffer_body message whose
        /// body bytes point at `resp.body` — `resp` MUST outlive the returned
        /// message and its write. Field nodes come from `resp`'s allocator (the
        /// request arena on the hot path, new_delete on the error paths).
        Http11Response make_beast_response(Response& resp);

        bool is_malformed_request(const boost::beast::error_code& ec) noexcept;
    }  // namespace detail

    /**
     * @brief HTTP/1.1 driver over Boost.Beast (spec §6.3).
     *
     * One keep-alive session loop per connection: parse (body into the request
     * arena), build a RequestContext, dispatch through the Router, stamp
     * Date/Server, flat-serialize the Response into a per-connection batch
     * buffer that flushes once the input runs dry (one write per pipelined
     * BATCH). Reads still go through beast's parser; writes bypass beast's
     * serializer entirely (see serialize_response). The bug-fix battery lands
     * here (body/header limits, per-phase deadlines via set_deadline_after +
     * the listener's deadline_watchdog, handler-exception → 500,
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

        template <IsStreamConnection ConnT>
        boost::asio::awaitable<void> serve(ConnT& conn, Router& router);

    private:
        static void stamp_common_headers(Response& resp);

        /// Flat-render `resp` (status line, headers, Content-Length,
        /// Connection: close when !keep_alive, CRLF, body) APPENDING to `out`.
        /// Byte-identical to what beast's serializer emitted; measured ~15%
        /// cheaper per request than http::async_write's lazy buffers_cat /
        /// buffers_suffix view machinery (~11% of ALL cycles at depth 1).
        static void serialize_response(Response& resp, std::string& out);

        /// Write `out` fully and clear it (cleared even on error — callers
        /// break/close, nothing may resend the bytes).
        template <typename Stream>
        static boost::asio::awaitable<boost::beast::error_code>
        flush(Stream& stream, std::string& out, boost::asio::cancellation_slot slot);

        Http11Config cfg_;
        // No SCROLL_COMPONENT_PREFIX / COMPONENT_LOG_* here: the h1 driver does
        // not log in v1, so it carries no Scroll dependency in its public header
        // (avoids a PUBLIC Scroll link just to satisfy the macro in consumers).
        // The h2/h3 scaffolds DO log, and link Scroll INTERFACE accordingly.
    };


    template <IsStreamConnection ConnT>
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

        // Session-scope response accumulator: pipelined requests batch their
        // responses here and flush with ONE write once the input buffer runs
        // dry (drogon's model; writes were 1.02 sendmsg per RESPONSE before,
        // 0.06 per response after — the whole of the former 12x gap at
        // pipeline depth 16). Plain heap string, NOT arena: it must survive
        // per-request arena resets; capacity amortizes across the session.
        std::string outbuf;
        outbuf.reserve(4096);

        while (true) {
            conn.reset_request_arena();
            std::pmr::polymorphic_allocator<char> arena_alloc{conn.arena_alloc().resource()};

            // Body AND fields out of the arena. The parser is a loop-scope local,
            // so it is destroyed at the end of this iteration — before the next
            // iteration's reset_request_arena() invalidates the arena block.
            detail::Http11Parser parser{
                std::piecewise_construct, std::forward_as_tuple(arena_alloc), std::forward_as_tuple(arena_alloc)};
            parser.header_limit(static_cast<std::uint32_t>(cfg_.max_header_bytes));
            parser.body_limit(cfg_.max_body_bytes);

            boost::beast::error_code ec;

            // Phase 1: header (also the idle wait between keep-alive requests).
            // Deadline STORE, not beast expires_after: the per-op stream timeout
            // armed timer.async_wait + cancel + an aborted-handler dispatch
            // around every read AND write — removing it measured +18% rps at
            // pipeline depth 1. The listener's deadline_watchdog enforces this
            // at ~500ms granularity (config timeouts are seconds).
            // TODO: cfg_.idle_timeout is currently unused — maybe wire it in here so
            //  a kept-alive connection waiting for its NEXT request uses idle_timeout,
            //  while header_timeout bounds only a header that is already mid-arrival.
            conn.set_deadline_after(cfg_.header_timeout);
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
                serialize_response(r, outbuf);  // post-loop flush sends it in order
                break;
            }
            // Beast checks Content-Length against body_limit eagerly at header-parse
            // time; the error surfaces here, not in phase-2 async_read.
            if (ec == http::error::body_limit) {
                Response r   = ResponseFactory::payload_too_large();
                r.keep_alive = false;
                stamp_common_headers(r);
                serialize_response(r, outbuf);  // post-loop flush sends it in order
                break;
            }
            if (ec) {
                if (detail::is_malformed_request(ec)) {
                    Response r   = ResponseFactory::bad_request("Bad Request");
                    r.keep_alive = false;
                    stamp_common_headers(r);
                    serialize_response(r, outbuf);  // post-loop flush sends it in order
                }
                break;  // malformed → 400 (written above); transport error → just close
            }

            // Phase 2: body — skipped entirely when the header already completed
            // the message (GET/HEAD/DELETE with no Content-Length and no chunked
            // encoding, i.e. most requests). async_read on a done parser is not
            // free: it still builds a composed-operation state on the heap and
            // still re-enters the socket. Measured at 1.24 recvmsg per request
            // against Drogon's 1.02, plus one allocation.
            if (!parser.is_done()) {
                conn.set_deadline_after(cfg_.body_timeout);
                co_await http::async_read(
                    stream,
                    buffer,
                    parser,
                    asio::bind_cancellation_slot(conn.cancel_slot(), asio::redirect_error(asio::use_awaitable, ec)));
            }
            if (ec == http::error::body_limit) {
                Response r   = ResponseFactory::payload_too_large();
                r.keep_alive = false;
                stamp_common_headers(r);
                serialize_response(r, outbuf);  // post-loop flush sends it in order
                break;
            }
            if (ec) {
                if (detail::is_malformed_request(ec)) {
                    Response r   = ResponseFactory::bad_request("Bad Request");
                    r.keep_alive = false;
                    stamp_common_headers(r);
                    serialize_response(r, outbuf);  // post-loop flush sends it in order
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

            // Batching: flush only when no more pipelined bytes are buffered
            // (or keep-alive ends / the batch cap is reached).
            // buffer.size() > 0 means the next request is at least partially
            // here already — parse it first, answer the whole batch with ONE
            // write. A trickle sender can delay the batch only until its own
            // bytes complete; the header deadline above bounds that window.
            serialize_response(response, outbuf);
            if (!keep_alive || buffer.size() == 0 || outbuf.size() >= 256 * 1024) {
                if (const boost::beast::error_code write_ec = co_await flush(stream, outbuf, conn.cancel_slot());
                    write_ec || !keep_alive)
                    break;
            }
        }
        // Flush anything still batched (error paths serialize without writing;
        // a transport error mid-flush already cleared outbuf).
        if (!outbuf.empty())
            co_await flush(stream, outbuf, conn.cancel_slot());
        co_await conn.async_close();
    }

    // Body is APPENDED (not gather-written) so a pipelined batch flushes as
    // ONE contiguous buffer; for the small-response regime this copy is far
    // cheaper than beast's per-write view walking. Revisit for large bodies
    // (a size threshold could gather-write the body instead of copying).
    inline void Http11Driver::serialize_response(Response& resp, std::string& out) {
        namespace http = boost::beast::http;
        out.append(resp.version == HttpVersion::http_1_0 ? "HTTP/1.0 " : "HTTP/1.1 ");
        char nbuf[20];
        const auto code    = static_cast<unsigned>(resp.status);
        const auto [cp, _] = std::to_chars(nbuf, nbuf + sizeof nbuf, code);
        out.append(nbuf, cp);
        out.push_back(' ');
        const auto reason = http::obsolete_reason(static_cast<http::status>(code));
        out.append(reason.data(), reason.size());
        out.append("\r\n");
        for (const auto& [name, value] : resp.headers) {
            out.append(name);
            out.append(": ");
            out.append(value);
            out.append("\r\n");
        }
        const std::string_view body = resp.body.buffered_view().value_or(std::string_view{});
        out.append("Content-Length: ");
        const auto [lp, _2] = std::to_chars(nbuf, nbuf + sizeof nbuf, body.size());
        out.append(nbuf, lp);
        out.append("\r\n");
        if (!resp.keep_alive)
            out.append("Connection: close\r\n");
        out.append("\r\n");
        out.append(body);
    }

    template <typename Stream>
    boost::asio::awaitable<boost::beast::error_code>
    Http11Driver::flush(Stream& stream, std::string& out, const boost::asio::cancellation_slot slot) {
        namespace asio = boost::asio;
        boost::beast::error_code ec;
        co_await asio::async_write(stream,
                                   asio::buffer(out.data(), out.size()),
                                   asio::bind_cancellation_slot(slot, asio::redirect_error(asio::use_awaitable, ec)));
        out.clear();  // cleared even on error: callers break/close, nothing may resend it
        co_return ec;
    }
}  // namespace demiplane::http
