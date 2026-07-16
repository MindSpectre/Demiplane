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
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/http.hpp>
#include <connection_concepts.hpp>
#include <http_date.hpp>
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
     * the tracker's deadline sweep, handler-exception → 500,
     * cancellation-aware I/O).
     */
    class Http11Driver {
    public:
        template <typename Http11ConfigTp>
            requires std::is_same_v<std::remove_cvref_t<Http11ConfigTp>, Http11Config>
        explicit Http11Driver(Http11ConfigTp&& cfg) noexcept
            : cfg_{std::forward<Http11ConfigTp>(cfg)} {
        }

        [[nodiscard]] static constexpr Protocol id() noexcept {
            return Protocol::http1;
        }

        [[nodiscard]] static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view ALPNS[] = {"http/1.1"};
            return ALPNS;
        }

        template <IsStreamConnection ConnT>
        AsyncVoid serve(ConnT& conn, Router& router);

    private:
        /// Flat-render `resp` (status line, headers, Date/Server when the
        /// handler didn't set them, Content-Length, Connection: close when !
        /// keep_alive, CRLF, body) APPENDING to `out`. Byte-identical to what
        /// beast's serializer emitted; measured ~15% cheaper per request than
        /// http::async_write's lazy buffers_cat / buffers_suffix view
        /// machinery (~11% of ALL cycles at depth 1). Date/Server presence is
        /// detected during the header pass we already do — folding the
        /// stamping here removed 2 contains + 2 set_header linear scans and
        /// the arena copies of both values per response. Only possible throw
        /// is an unrecoverable bad_alloc growing `out`
        /// (GEARS_UNRECOVERABLE_NOEXCEPT — terminate by default).
        static void serialize_response(Response& resp, std::string& out) GEARS_UNRECOVERABLE_NOEXCEPT;

        Http11Config cfg_;
        // No SCROLL_COMPONENT_PREFIX / COMPONENT_LOG_* here: the h1 driver does
        // not log in v1, so it carries no Scroll dependency in its public header
        // (avoids a PUBLIC Scroll link just to satisfy the macro in consumers).
        // The h2/h3 scaffolds DO log, and link Scroll INTERFACE accordingly.
    };


    template <IsStreamConnection ConnT>
    AsyncVoid Http11Driver::serve(ConnT& conn, Router& router) {
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

            // Parse directly from buffered bytes; hit the socket ONLY when the
            // parser reports need_more. beast's async_read_header / async_read
            // composed ops built and dispatched an operation state per MESSAGE
            // — measured at ~10k cycles/request against the raw-asio floor
            // (README Finding 7). parser.put() is the same parser without the
            // ceremony; eager() folds the body into the same loop instead of a
            // separate phase-2 op.
            //
            // Deadlines (STORE, not beast expires_after — Finding 4), armed at
            // the points where this coroutine can actually park in a read:
            //   - message start with an empty buffer = the keep-alive idle wait
            //     → idle_timeout; buffered pipelined bytes mean the message is
            //     already mid-arrival → header_timeout;
            //   - parking again with a partial message → header/body timeout,
            //     armed ONCE per phase (absolute phase deadlines — a
            //     byte-per-read trickler cannot roll its window forever).
            // A message that never parks mid-arrival (the overwhelmingly common
            // case) arms exactly ONE deadline: set_deadline_after reads the
            // clock, and a second read per request is measurable at this rate.
            // The last-armed deadline also covers dispatch + write, as the old
            // per-phase expires_after did.
            parser.eager(true);
            bool header_deadline_set = buffer.size() > 0;
            bool body_deadline_set   = false;
            conn.set_deadline_after(header_deadline_set ? cfg_.header_timeout : cfg_.idle_timeout);
            while (!parser.is_done()) {
                if (buffer.size() > 0) {
                    boost::beast::error_code pec;
                    const std::size_t parsed = parser.put(buffer.cdata(), pec);
                    buffer.consume(parsed);
                    if (pec && pec != http::error::need_more) [[unlikely]] {
                        ec = pec;  // header_limit / body_limit / the malformed set
                        break;
                    }
                    if (parser.is_done()) [[likely]]
                        break;  // pipelined leftovers stay in `buffer` for the next iteration
                }
                // About to park mid-message: swap the idle deadline for the
                // phase deadline that governs this wait.
                if (parser.got_some()) [[unlikely]] {
                    if (parser.is_header_done() && !body_deadline_set) {
                        conn.set_deadline_after(cfg_.body_timeout);
                        body_deadline_set = true;
                    } else if (!parser.is_header_done() && !header_deadline_set) {
                        conn.set_deadline_after(cfg_.header_timeout);
                        header_deadline_set = true;
                    }
                }
                boost::beast::error_code rec;
                const std::size_t n = co_await stream.async_read_some(
                    buffer.prepare(16384),
                    asio::bind_cancellation_slot(conn.cancel_slot(), asio::redirect_error(use_strand_awaitable, rec)));
                if (rec) [[unlikely]] {
                    // Reproduce the composed ops' eof mapping: eof on a fresh
                    // message is a clean keep-alive close; eof mid-message is
                    // a truncated request. Everything else surfaces as-is
                    // (operation_aborted from cancel(), resets, ...).
                    if (rec == asio::error::eof)
                        ec = parser.got_some() ? http::error::partial_message : http::error::end_of_stream;
                    else
                        ec = rec;
                    break;
                }
                buffer.commit(n);
            }

            if (ec == http::error::end_of_stream) [[unlikely]]
                break;  // client closed cleanly between requests
            if (ec == http::error::header_limit) [[unlikely]] {
                Response r   = ResponseFactory::bad_request("Request Header Fields Too Large");
                r.keep_alive = false;
                serialize_response(r, outbuf);  // post-loop flush sends it in order
                break;
            }
            // Beast checks Content-Length against body_limit eagerly at
            // header-parse time, and streamed/chunked overruns surface later —
            // both land here as the same error.
            if (ec == http::error::body_limit) [[unlikely]] {
                Response r   = ResponseFactory::payload_too_large();
                r.keep_alive = false;
                serialize_response(r, outbuf);  // post-loop flush sends it in order
                break;
            }
            if (ec) [[unlikely]] {
                if (detail::is_malformed_request(ec)) {
                    Response r   = ResponseFactory::bad_request("Bad Request");
                    r.keep_alive = false;
                    serialize_response(r, outbuf);  // post-loop flush sends it in order
                }
                break;  // malformed → 400 (written above); transport error → just close
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

            // Batching: flush only when no more pipelined bytes are buffered
            // (or keep-alive ends / the batch cap is reached).
            // buffer.size() > 0 means the next request is at least partially
            // here already — parse it first, answer the whole batch with ONE
            // write. A trickle sender can delay the batch only until its own
            // bytes complete; the header deadline above bounds that window.
            serialize_response(response, outbuf);
            if (!keep_alive || buffer.size() == 0 || outbuf.size() >= 256 * 1024) {
                // Inline write (not a flush() coroutine member): a whole
                // awaitable frame + pump per request just to wrap one
                // async_write was measurable at depth 1.
                boost::beast::error_code write_ec;
                co_await asio::async_write(
                    stream,
                    asio::buffer(outbuf.data(), outbuf.size()),
                    asio::bind_cancellation_slot(conn.cancel_slot(),
                                                 asio::redirect_error(use_strand_awaitable, write_ec)));
                outbuf.clear();  // cleared even on error: nothing may resend it
                if (write_ec || !keep_alive) [[unlikely]]
                    break;
            }
        }
        // Flush anything still batched (error paths serialize without writing;
        // a transport error mid-write already cleared outbuf).
        if (!outbuf.empty()) [[unlikely]] {
            boost::beast::error_code write_ec;  // swallowed: we are closing either way
            co_await asio::async_write(
                stream,
                asio::buffer(outbuf.data(), outbuf.size()),
                asio::bind_cancellation_slot(conn.cancel_slot(), asio::redirect_error(use_strand_awaitable, write_ec)));
            outbuf.clear();
        }
        co_await conn.async_close();
    }

    // Body is APPENDED (not gather-written) so a pipelined batch flushes as
    // ONE contiguous buffer; for the small-response regime this copy is far
    // cheaper than beast's per-write view walking. Revisit for large bodies
    // (a size threshold could gather-write the body instead of copying).
    inline void Http11Driver::serialize_response(Response& resp, std::string& out) GEARS_UNRECOVERABLE_NOEXCEPT {
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
        bool has_date = false, has_server = false;
        for (const auto& [name, value] : resp.headers) {
            if (!has_date && boost::beast::iequals(name, "Date"))
                has_date = true;
            else if (!has_server && boost::beast::iequals(name, "Server"))
                has_server = true;
            out.append(name);
            out.append(": ");
            out.append(value);
            out.append("\r\n");
        }
        if (!has_date) [[likely]] {
            out.append("Date: ");
            const std::string_view date = imf_fixdate_now();
            out.append(date.data(), date.size());
            out.append("\r\n");
        }
        if (!has_server) [[likely]]
            out.append("Server: Demiplane\r\n");
        const std::string_view body = resp.body.buffered_view().value_or(std::string_view{});
        out.append("Content-Length: ");
        const auto [lp, _2] = std::to_chars(nbuf, nbuf + sizeof nbuf, body.size());
        out.append(nbuf, lp);
        out.append("\r\n");
        if (!resp.keep_alive) [[unlikely]]
            out.append("Connection: close\r\n");
        out.append("\r\n");
        out.append(body);
    }

}  // namespace demiplane::http
