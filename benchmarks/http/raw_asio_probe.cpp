// H3 CONTROL: minimal raw-asio HTTP/1.1 server matching demiplane's topology —
// one shared io_context, N worker threads, strand-bound socket per connection,
// one awaitable coroutine per connection — but ZERO beast, zero framework:
// hand parser (find header terminator), flat serialization, response batching,
// no timers. Measures the asio+coroutine+strand floor on this box.
//
// Not wired into CMake — build standalone against the vcpkg boost headers:
//   clang++ -O3 -std=c++23 -stdlib=libc++ -DNDEBUG \
//     -I build/bench/vcpkg_installed/x64-linux-clang/include \
//     benchmarks/http/raw_asio_probe.cpp -o raw_asio_probe -pthread -fuse-ld=mold
//
//   ./raw_asio_probe [port=8090] [threads=4]
//
// Measured 2026-07-10 (README Finding 7): 481k rps at pipeline 1 (= drogon),
// 5.84M at pipeline 16 (1.7x drogon). demiplane's remaining gap is beast's
// read path + framework glue, not asio.
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

namespace asio = boost::asio;
using Executor = asio::io_context::executor_type;
using Strand   = asio::strand<Executor>;
using Socket   = asio::basic_stream_socket<asio::ip::tcp, Strand>;
using Acceptor = asio::basic_socket_acceptor<asio::ip::tcp, Executor>;

static void render_date(char* buf) {  // "Fri, 10 Jul 2026 07:39:00 GMT"
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
    std::string in;  // rolling input buffer
    in.reserve(8192);
    std::string out;  // batched responses
    out.reserve(4096);
    char tmp[16384];
    char date[40];

    boost::system::error_code ec;
    for (;;) {
        // parse every complete request already buffered; batch the responses
        std::size_t consumed = 0;
        for (;;) {
            const auto end = in.find("\r\n\r\n", consumed);
            if (end == std::string::npos)
                break;
            // request line: METHOD SP TARGET SP VERSION — we answer /ping only,
            // like the bench controller (no body handling: GET-only load).
            render_date(date);
            out.append("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nDate: ");
            out.append(date);
            out.append("\r\nServer: Demiplane\r\nContent-Length: 4\r\n\r\npong");
            consumed = end + 4;
        }
        if (consumed) {
            in.erase(0, consumed);  // keep partial tail
            const auto wec = co_await [&]() -> asio::awaitable<boost::system::error_code> {
                boost::system::error_code w;
                co_await asio::async_write(sock, asio::buffer(out), asio::redirect_error(asio::use_awaitable, w));
                co_return w;
            }();
            out.clear();
            if (wec)
                break;
        }
        const std::size_t n =
            co_await sock.async_read_some(asio::buffer(tmp, sizeof tmp), asio::redirect_error(asio::use_awaitable, ec));
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
        Strand strand = asio::make_strand(ioc.get_executor());
        boost::system::error_code ec;
        Socket sock = co_await acc.async_accept(strand, asio::redirect_error(asio::use_awaitable, ec));
        if (ec)
            break;
        boost::system::error_code nd;
        sock.set_option(asio::ip::tcp::no_delay(true), nd);
        asio::co_spawn(strand, session(std::move(sock)), asio::detached);
    }
}

int main(int argc, char* argv[]) {
    const std::uint16_t port = argc > 1 ? static_cast<std::uint16_t>(std::atoi(argv[1])) : 8090;
    const int threads        = argc > 2 ? std::atoi(argv[2]) : 4;

    asio::io_context ioc{threads};
    Acceptor acc{ioc.get_executor()};
    const asio::ip::tcp::endpoint ep{asio::ip::address_v4::any(), port};
    acc.open(ep.protocol());
    acc.set_option(asio::socket_base::reuse_address(true));
    acc.bind(ep);
    acc.listen(asio::socket_base::max_listen_connections);

    asio::co_spawn(ioc, accept_loop(ioc, acc), asio::detached);

    std::printf("raw_asio_probe on :%u, %d threads\n", port, threads);
    std::vector<std::thread> workers;
    for (int i = 1; i < threads; ++i)
        workers.emplace_back([&] { ioc.run(); });
    ioc.run();
    for (auto& w : workers)
        w.join();
    return 0;
}
