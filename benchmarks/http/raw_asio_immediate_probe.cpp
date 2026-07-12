// EXPERIMENT: raw_asio_perthread_probe + IMMEDIATE EXECUTORS (asio 1.30+).
// Identical to the per-thread probe except every socket token is wrapped in
// bind_immediate_executor(<own context executor>, ...): async ops that
// complete speculatively at initiation (data already readable / socket
// writable — the common case under load) dispatch their completion INLINE
// on the initiating thread instead of being posted through the scheduler
// queue. Isolates ONE variable vs raw_asio_perthread_probe: the post-per-op
// scheduler round-trip on speculative completions.
//
// Build standalone:
//   clang++ -O3 -std=c++23 -stdlib=libc++ -DNDEBUG \
//     -I build/bench/vcpkg_installed/x64-linux-clang/include \
//     benchmarks/http/raw_asio_immediate_probe.cpp -o raw_asio_immediate_probe \
//     -pthread -fuse-ld=mold
//
//   ./raw_asio_immediate_probe [port=8093] [threads=4]
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/bind_immediate_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

namespace asio = boost::asio;
using Executor = asio::io_context::executor_type;
using Socket   = asio::basic_stream_socket<asio::ip::tcp, Executor>;
using Acceptor = asio::basic_socket_acceptor<asio::ip::tcp, Executor>;

static void render_date(char* buf) {
    static constexpr const char* days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static constexpr const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::snprintf(buf,
                  40,
                  "%s, %02d %s %04d %02d:%02d:%02d GMT",
                  days[tm.tm_wday],
                  tm.tm_mday,
                  months[tm.tm_mon],
                  tm.tm_year + 1900,
                  tm.tm_hour,
                  tm.tm_min,
                  tm.tm_sec);
}

static asio::awaitable<void> session(Socket sock) {
    std::string in;
    in.reserve(8192);
    std::string out;
    out.reserve(4096);
    char tmp[16384];
    char date[40];

    boost::system::error_code ec;
    // Immediate executor = the socket's own single-runner context executor:
    // dispatch from its own thread runs the completion inline.
    const auto imm = sock.get_executor();
    for (;;) {
        std::size_t consumed = 0;
        for (;;) {
            const auto end = in.find("\r\n\r\n", consumed);
            if (end == std::string::npos)
                break;
            render_date(date);
            out.append("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nDate: ");
            out.append(date);
            out.append("\r\nServer: Demiplane\r\nContent-Length: 4\r\n\r\npong");
            consumed = end + 4;
        }
        if (consumed) {
            in.erase(0, consumed);
            boost::system::error_code w;
            co_await asio::async_write(
                sock,
                asio::buffer(out),
                asio::bind_immediate_executor(imm, asio::redirect_error(asio::use_awaitable, w)));
            out.clear();
            if (w)
                break;
        }
        const std::size_t n = co_await sock.async_read_some(
            asio::buffer(tmp, sizeof tmp),
            asio::bind_immediate_executor(imm, asio::redirect_error(asio::use_awaitable, ec)));
        if (ec)
            break;
        in.append(tmp, n);
    }
    boost::system::error_code ignore;
    sock.shutdown(asio::ip::tcp::socket::shutdown_both, ignore);
    sock.close(ignore);
}

static asio::awaitable<void> accept_loop(asio::io_context& ioc, Acceptor& acc) {
    for (;;) {
        boost::system::error_code ec;
        Socket sock = co_await acc.async_accept(ioc.get_executor(), asio::redirect_error(asio::use_awaitable, ec));
        if (ec)
            break;
        boost::system::error_code nd;
        sock.set_option(asio::ip::tcp::no_delay(true), nd);
        asio::co_spawn(ioc.get_executor(), session(std::move(sock)), asio::detached);
    }
}

int main(int argc, char* argv[]) {
    const std::uint16_t port = argc > 1 ? static_cast<std::uint16_t>(std::atoi(argv[1])) : 8093;
    const int threads        = argc > 2 ? std::atoi(argv[2]) : 4;

    std::printf("raw_asio_immediate_probe on :%u, %d io_contexts, IMMEDIATE completions\n", port, threads);

    std::vector<std::thread> workers;
    for (int i = 0; i < threads; ++i) {
        workers.emplace_back([port] {
            asio::io_context ioc{1};  // ← the variable under test: one loop per thread
            Acceptor acc{ioc.get_executor()};
            const asio::ip::tcp::endpoint ep{asio::ip::address_v4::any(), port};
            acc.open(ep.protocol());
            acc.set_option(asio::socket_base::reuse_address(true));
            const int one = 1;
            ::setsockopt(acc.native_handle(), SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
            acc.bind(ep);
            acc.listen(asio::socket_base::max_listen_connections);
            asio::co_spawn(ioc, accept_loop(ioc, acc), asio::detached);
            ioc.run();
        });
    }
    for (auto& w : workers)
        w.join();
    return 0;
}
