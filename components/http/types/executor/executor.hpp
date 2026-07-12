#pragma once

#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/basic_stream.hpp>
#include <boost/beast/core/rate_policy.hpp>

namespace demiplane::http {

    /**
     * @brief The concrete executor types the server runs on.
     *
     * These were `boost::asio::any_io_executor` until the throughput work.
     * `any_io_executor` is a type-erased executor: every dispatch through it is
     * a virtual call plus shared-ptr refcounting on the target. That is
     * tolerable at setup and shutdown, but `beast::tcp_stream` is
     * `basic_stream<tcp, any_io_executor, ...>` — so the erasure sat on the
     * REQUEST hot path, in every async_read_header / async_read / async_write of
     * the h1 session loop. `perf` attributed 14.25% of all cycles to it
     * (any_executor_base::move_shared, shared_target_executor, can_prefer,
     * executor_work_guard<any_io_executor>, ...), against 0% for Drogon.
     *
     * Fixing this costs the ability to drive the Server from a `thread_pool` or
     * other foreign executor. Nothing in the tree ever did — every construction
     * site already passes `io_context::executor_type` via `ioc.get_executor()`.
     *
     * CONCURRENCY CONTRACT (README Finding 13): each injected executor must be
     * driven by AT MOST ONE thread ("io_context per worker thread"). `Strand`
     * — historically `strand<Executor>`, serializing one connection's I/O on a
     * shared multi-threaded context — is now an alias for the bare executor:
     * the single runner of the connection's home context IS the serialization.
     * Real strands measurably hurt this topology (every completion pays a
     * strand-queue round-trip that shared-context work-stealing used to hide);
     * the alias is kept so connection/driver/handler types keep reading
     * "Strand" where per-connection serialization is meant.
     */
    using Executor = boost::asio::io_context::executor_type;
    using Strand   = Executor;

    /// Accepted socket + the beast stream wrapping it. Both are bound to the
    /// connection's Strand, so the driver's I/O dispatches straight to it.
    using Socket = boost::asio::basic_stream_socket<boost::asio::ip::tcp, Strand>;
    using Stream = boost::beast::basic_stream<boost::asio::ip::tcp, Strand, boost::beast::unlimited_rate_policy>;

    /// The acceptor lives at listener scope, not connection scope, so it takes
    /// the bare executor. `async_accept(strand, ...)` is what binds the accepted
    /// socket to a Strand.
    using Acceptor = boost::asio::basic_socket_acceptor<boost::asio::ip::tcp, Executor>;

    /// use_awaitable token typed to the connection Strand. The default
    /// `asio::use_awaitable` is `use_awaitable_t<any_io_executor>` — every
    /// co_awaited op inside the request loop would RE-erase the coroutine
    /// frame to `any_io_executor` (visible in perf as
    /// `awaitable_thread<any_io_executor>::pump` + `any_executor_base::
    /// execute`), undoing the concrete-executor typing above. Request-path
    /// coroutines are `awaitable<T, Strand>` and must use this token.
    /// Setup/shutdown-scope coroutines (accept loops, graceful drain,
    /// observers) stay on plain `use_awaitable` — they are not hot.
    inline constexpr boost::asio::use_awaitable_t<Strand> use_strand_awaitable{};

}  // namespace demiplane::http
