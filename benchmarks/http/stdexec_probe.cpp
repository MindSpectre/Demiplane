// FLOOR PROBE: minimal HTTP/1.1 server on stdexec (std::execution reference
// implementation) over its io_uring context — the sender/receiver counterpart
// of raw_asio_probe.cpp. Same protocol behavior: find "\r\n\r\n", canned
// 124-byte response with a per-response Date render, response batching.
//
// TOPOLOGY NOTE (differs from raw_asio_probe on purpose, document in README):
// stdexec's io_uring_context is a single-runner ring, so this probe runs one
// context per thread with SO_REUSEPORT listeners (drogon-style), NOT the
// shared-context + strands topology. It therefore measures stdexec's model at
// its natural best; the asio probe measures OUR topology. Both floors answer
// "is the async framework the bottleneck", not "which topology demiplane uses".
//
// stdexec has NO public socket senders (sockets live in the immature `sio`
// library) — the accept/recv/send senders below are hand-rolled against the
// context's internal __io_task_facade extension seam (the same mechanism its
// own timers use). Probe-quality code, deliberately not production.
//
// Build (standalone; stdexec is header-only, no liburing needed — it talks to
// the ring via raw syscalls + <linux/io_uring.h>):
//   git clone --depth 1 https://github.com/NVIDIA/stdexec /tmp/stdexec
//   clang++ -O3 -std=c++23 -stdlib=libc++ -DNDEBUG -I /tmp/stdexec/include \
//     benchmarks/http/stdexec_probe.cpp -o stdexec_probe -pthread -fuse-ld=mold
//
//   ./stdexec_probe [port=8091] [threads=4]
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <exec/async_scope.hpp>
#include <exec/linux/io_uring_context.hpp>
#include <exec/task.hpp>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexec/execution.hpp>
#include <sys/socket.h>
#include <unistd.h>

namespace ex = stdexec;
using IoCtx  = exec::io_uring_context;

namespace {

    // ── hand-rolled io_uring socket senders over the __io_task_facade seam ──
    struct IoSpec {
        std::uint8_t opcode;
        int fd;
        void* ptr;
        unsigned len;
    };

    template <class Receiver>
    struct SqeOpBase {
        exec::__io_uring::__context& ctx_;
        IoSpec spec_;
        Receiver rcvr_;

        SqeOpBase(exec::__io_uring::__context& ctx, const IoSpec spec, Receiver rcvr)
            : ctx_{ctx},
              spec_{spec},
              rcvr_{std::move(rcvr)} {
        }

        [[nodiscard]] auto context() const noexcept -> exec::__io_uring::__context& {
            return ctx_;
        }
        [[nodiscard]] static auto ready() noexcept -> bool {
            return false;  // always submits an SQE
        }
        void submit(::io_uring_sqe& sqe) noexcept {
            // Field assignments only — the context owns user_data.
            sqe.opcode = spec_.opcode;
            sqe.fd     = spec_.fd;
            sqe.addr   = reinterpret_cast<__u64>(spec_.ptr);
            sqe.len    = spec_.len;
        }
        void complete(const ::io_uring_cqe& cqe) noexcept {
            if (cqe.res >= 0)
                ex::set_value(std::move(rcvr_), cqe.res);
            else
                ex::set_error(std::move(rcvr_),
                              std::make_exception_ptr(std::system_error{-cqe.res, std::system_category()}));
        }
    };

    struct SqeSender {
        using sender_concept = ex::sender_t;
        using completion_signatures =
            ex::completion_signatures<ex::set_value_t(int), ex::set_error_t(std::exception_ptr)>;
        exec::__io_uring::__context* ctx;
        IoSpec spec;

        template <class Receiver>
        auto connect(Receiver rcvr) && noexcept {
            return exec::__io_uring::__io_task_facade<SqeOpBase<Receiver>>{std::in_place, *ctx, spec, std::move(rcvr)};
        }
    };

    SqeSender async_accept(IoCtx& ctx, const int listen_fd) {
        return {
            &ctx, {IORING_OP_ACCEPT, listen_fd, nullptr, 0}
        };
    }
    SqeSender async_recv(IoCtx& ctx, const int fd, void* buf, const unsigned len) {
        return {
            &ctx, {IORING_OP_RECV, fd, buf, len}
        };
    }
    SqeSender async_send(IoCtx& ctx, const int fd, const void* buf, const unsigned len) {
        return {
            &ctx, {IORING_OP_SEND, fd, const_cast<void*>(buf), len}
        };
    }

    // ── the same trivial HTTP behavior as raw_asio_probe ─────────────────────
    void render_date(char* buf) {
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

    exec::task<void> session(IoCtx& ctx, const int fd) {
        std::string in;
        in.reserve(8192);
        std::string out;
        out.reserve(4096);
        char tmp[16384];
        char date[40];

        try {
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
                    std::size_t sent = 0;
                    while (sent < out.size()) {
                        const int n =
                            co_await async_send(ctx, fd, out.data() + sent, static_cast<unsigned>(out.size() - sent));
                        sent += static_cast<std::size_t>(n);
                    }
                    out.clear();
                }
                const int n = co_await async_recv(ctx, fd, tmp, sizeof tmp);
                if (n == 0)
                    break;  // peer closed
                in.append(tmp, static_cast<std::size_t>(n));
            }
        } catch (...) {  // reset / cancel — just close
        }
        ::close(fd);
    }

    exec::task<void> accept_loop(IoCtx& ctx, const int listen_fd, exec::async_scope& scope) {
        try {
            for (;;) {
                const int fd  = co_await async_accept(ctx, listen_fd);
                const int one = 1;
                ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
                scope.spawn(session(ctx, fd));
            }
        } catch (...) {}
    }

    int make_listener(const std::uint16_t port) {
        const int fd  = ::socket(AF_INET, SOCK_STREAM, 0);
        const int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);  // one queue per thread/context
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(port);
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || ::listen(fd, 4096) != 0) {
            std::perror("bind/listen");
            std::exit(1);
        }
        return fd;
    }

}  // namespace

int main(int argc, char* argv[]) {
    const std::uint16_t port = argc > 1 ? static_cast<std::uint16_t>(std::atoi(argv[1])) : 8091;
    const int threads        = argc > 2 ? std::atoi(argv[2]) : 4;

    std::printf("stdexec_probe on :%u, %d io_uring contexts (SO_REUSEPORT)\n", port, threads);

    std::vector<std::thread> workers;
    for (int i = 0; i < threads; ++i) {
        workers.emplace_back([port] {
            IoCtx ctx;
            exec::async_scope scope;
            const int listen_fd = make_listener(port);
            scope.spawn(accept_loop(ctx, listen_fd, scope));
            ctx.run_until_stopped();
        });
    }
    for (auto& w : workers)
        w.join();
    return 0;
}
