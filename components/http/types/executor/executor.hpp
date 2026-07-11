#pragma once

#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
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
     * Note the two roles are distinct and NOT interchangeable: `Executor` is the
     * bare io_context executor (acceptors, timers, co_spawn at server scope);
     * `Strand` is what serializes one connection's I/O. A single alias for both
     * will not compile — `strand<E>` does not convert to `E`.
     */
    using Executor = boost::asio::io_context::executor_type;
    using Strand   = boost::asio::strand<Executor>;

    /// Accepted socket + the beast stream wrapping it. Both are bound to the
    /// connection's Strand, so the driver's I/O dispatches straight to it.
    using Socket = boost::asio::basic_stream_socket<boost::asio::ip::tcp, Strand>;
    using Stream = boost::beast::basic_stream<boost::asio::ip::tcp, Strand, boost::beast::unlimited_rate_policy>;

    /// The acceptor lives at listener scope, not connection scope, so it takes
    /// the bare executor. `async_accept(strand, ...)` is what binds the accepted
    /// socket to a Strand.
    using Acceptor = boost::asio::basic_socket_acceptor<boost::asio::ip::tcp, Executor>;

}  // namespace demiplane::http
