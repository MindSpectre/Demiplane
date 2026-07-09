/**
 * HTTP/1.1 saturating load generator.
 *
 * Drives a fixed request count over persistent keep-alive connections and
 * reports client-side throughput. Replaces the pre-PR-1 bomber, which opened a
 * fresh connection per request (`Connection: close`), blocked one request at a
 * time per thread, and slept between requests — it measured connection setup and
 * its own sleep, never HTTP serving throughput.
 *
 *   http_bomber --host 127.0.0.1 --port 8080 --path /ping \
 *               --threads 8 --conns 256 --pipeline 1 \
 *               --requests 100000000 --warmup 5000000 [--strict] [--json]
 *
 * Design notes:
 *   - io_context per thread. A shared io_context serializes on its scheduler
 *     lock, which caps the generator well below what the server can absorb.
 *   - Per-thread cache-line-isolated counters, published to the shared total
 *     once per 1024 requests. An atomic per request would make the client's own
 *     contention the thing under measurement.
 *   - Responses to a fixed-body endpoint are byte-identical, so the default
 *     read path validates the first response per connection in full and then
 *     matches expected byte counts. --strict re-validates the status line and
 *     framing of every response.
 *   - The clock is client-side only. Counting on the server would put a
 *     contended RMW on its hot path that the reference server does not carry.
 */
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

namespace net = boost::asio;
using tcp     = net::ip::tcp;

namespace {

    // ── Configuration ────────────────────────────────────────────────────────
    struct Options {
        std::string host       = "127.0.0.1";
        std::string port       = "8080";
        std::string path       = "/ping";
        std::size_t threads    = 8;
        std::size_t conns      = 256;
        std::size_t pipeline   = 1;
        std::uint64_t requests = 100'000'000;
        std::uint64_t warmup   = 5'000'000;
        bool strict            = false;
        bool json              = false;
    };

    // ── Latency histogram (log-linear, 1-in-64 sampling) ─────────────────────
    // 64 buckets per power of two, from 1us. Merged across threads at the end.
    constexpr std::size_t kSubBuckets = 64;
    constexpr std::size_t kMagnitudes = 24;  // up to ~16s
    constexpr std::size_t kBuckets    = kSubBuckets * kMagnitudes;

    std::size_t bucket_of(const std::uint64_t micros) noexcept {
        if (micros < kSubBuckets)
            return micros;
        const auto mag        = static_cast<std::size_t>(63 - __builtin_clzll(micros));
        const auto sub        = static_cast<std::size_t>((micros >> (mag - 6)) & (kSubBuckets - 1));
        const std::size_t idx = (mag - 5) * kSubBuckets + sub;
        return std::min(idx, kBuckets - 1);
    }

    std::uint64_t bucket_value(const std::size_t idx) noexcept {
        if (idx < kSubBuckets)
            return idx;
        const std::size_t mag = idx / kSubBuckets + 5;
        const std::size_t sub = idx % kSubBuckets;
        return (static_cast<std::uint64_t>(sub) | kSubBuckets) << (mag - 6);
    }

    // ── Per-thread state, cache-line isolated ────────────────────────────────
    struct alignas(64) ThreadStats {
        std::uint64_t ok                = 0;
        std::uint64_t failed            = 0;
        std::uint64_t bytes             = 0;
        std::vector<std::uint64_t> hist = std::vector<std::uint64_t>(kBuckets, 0);
        char pad[64]{};
    };

    // ── Shared run state ─────────────────────────────────────────────────────
    struct RunState {
        std::atomic<std::uint64_t> published{0};  // requests completed, coarse
        std::atomic<bool> measuring{false};       // warmup done, clock running
        std::atomic<bool> stop{false};
        std::atomic<std::uint64_t> warm_published{0};
    };

    constexpr std::uint64_t kPublishEvery = 1024;

    // A single persistent keep-alive connection driving `pipeline` requests in
    // flight. Lives until the run stops; owned by a shared_ptr held by its own
    // async chain (standard asio ownership).
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        Connection(net::io_context& ioc,
                   const tcp::resolver::results_type& endpoints,
                   const Options& opt,
                   ThreadStats& stats,
                   RunState& run,
                   std::string_view request_block,
                   const unsigned sample_seed)
            : socket_(ioc),
              endpoints_(endpoints),
              opt_(opt),
              stats_(stats),
              run_(run),
              request_block_(request_block),
              sample_counter_(sample_seed % 64) {
            read_buf_.reserve(64 * 1024);
        }

        void start() {
            auto self = shared_from_this();
            net::async_connect(socket_, endpoints_, [self](const boost::system::error_code& ec, const tcp::endpoint&) {
                if (ec) {
                    self->stats_.failed += self->opt_.pipeline;
                    return;  // connection never enters the loop; run continues short-handed
                }
                self->socket_.set_option(tcp::no_delay(true));
                self->write();
            });
        }

    private:
        void write() {
            if (run_.stop.load(std::memory_order_relaxed))
                return;
            batch_start_ = std::chrono::steady_clock::now();
            auto self    = shared_from_this();
            net::async_write(socket_,
                             net::buffer(request_block_.data(), request_block_.size()),
                             [self](const boost::system::error_code& ec, std::size_t) {
                                 if (ec) {
                                     self->stats_.failed += self->opt_.pipeline;
                                     return;
                                 }
                                 self->read();
                             });
        }

        void read() {
            auto self = shared_from_this();
            socket_.async_read_some(net::buffer(chunk_), [self](const boost::system::error_code& ec, std::size_t n) {
                if (ec) {
                    self->stats_.failed += self->opt_.pipeline;
                    return;
                }
                self->read_buf_.append(self->chunk_.data(), n);
                self->consume();
            });
        }

        // Returns bytes of one complete response starting at `p`, or 0 if the
        // buffer does not yet hold a full response.
        std::size_t response_length(const std::string_view p) const {
            const std::size_t head = p.find("\r\n\r\n");
            if (head == std::string_view::npos)
                return 0;
            const std::size_t head_end     = head + 4;
            // Locate Content-Length. The bench endpoints never chunk.
            const std::string_view headers = p.substr(0, head_end);
            std::size_t cl_pos             = headers.find("\r\ncontent-length:");
            if (cl_pos == std::string_view::npos)
                cl_pos = headers.find("\r\nContent-Length:");
            if (cl_pos == std::string_view::npos)
                return 0;  // no body length we understand
            std::size_t v = cl_pos + std::strlen("\r\ncontent-length:");
            while (v < headers.size() && headers[v] == ' ')
                ++v;
            std::uint64_t len = 0;
            const auto* first = headers.data() + v;
            const auto* last  = headers.data() + headers.size();
            if (std::from_chars(first, last, len).ec != std::errc{})
                return 0;
            const std::size_t total = head_end + static_cast<std::size_t>(len);
            return p.size() >= total ? total : 0;
        }

        void consume() {
            std::size_t off = 0;
            while (completed_in_batch_ < opt_.pipeline) {
                const std::string_view rest{read_buf_.data() + off, read_buf_.size() - off};
                std::size_t len = 0;

                if (opt_.strict || expected_len_ == 0) {
                    len = response_length(rest);
                    if (len == 0)
                        break;
                    // First response on this connection establishes the shape;
                    // --strict re-checks the status line on every one.
                    if (opt_.strict || !validated_) {
                        if (rest.substr(0, 12) != "HTTP/1.1 200") {
                            stats_.failed += 1;
                            run_.stop.store(true, std::memory_order_relaxed);
                            return;
                        }
                    }
                    if (!validated_) {
                        validated_    = true;
                        expected_len_ = len;
                    }
                } else {
                    // Fast path: fixed-body endpoint, every response is
                    // byte-identical to the one we validated on this connection.
                    if (rest.size() < expected_len_)
                        break;
                    len = expected_len_;
                }

                off          += len;
                stats_.bytes += len;
                ++completed_in_batch_;
            }

            if (off > 0)
                read_buf_.erase(0, off);

            if (completed_in_batch_ < opt_.pipeline) {
                read();
                return;
            }
            completed_in_batch_ = 0;
            on_batch_done();
        }

        void on_batch_done() {
            const bool measuring = run_.measuring.load(std::memory_order_relaxed);

            // Latency: sample 1 in 64 batches, recorded per-request.
            if (measuring && (++sample_counter_ & 63u) == 0) {
                const auto elapsed = std::chrono::steady_clock::now() - batch_start_;
                const auto micros =
                    static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() /
                                               static_cast<std::int64_t>(opt_.pipeline));
                stats_.hist[bucket_of(micros)] += 1;
            }

            stats_.ok += opt_.pipeline;
            local_    += opt_.pipeline;

            if (local_ >= kPublishEvery) {
                if (measuring) {
                    const std::uint64_t total = run_.published.fetch_add(local_, std::memory_order_relaxed) + local_;
                    if (total >= opt_.requests) {
                        run_.stop.store(true, std::memory_order_release);
                        return;
                    }
                } else {
                    const std::uint64_t total =
                        run_.warm_published.fetch_add(local_, std::memory_order_relaxed) + local_;
                    if (total >= opt_.warmup)
                        run_.measuring.store(true, std::memory_order_release);
                }
                local_ = 0;
            }

            if (run_.stop.load(std::memory_order_acquire))
                return;
            write();
        }

        tcp::socket socket_;
        const tcp::resolver::results_type& endpoints_;
        const Options& opt_;
        ThreadStats& stats_;
        RunState& run_;
        std::string_view request_block_;

        std::array<char, 16 * 1024> chunk_{};
        std::string read_buf_;
        std::size_t completed_in_batch_ = 0;
        std::size_t expected_len_       = 0;
        bool validated_                 = false;
        std::uint64_t local_            = 0;
        unsigned sample_counter_        = 0;
        std::chrono::steady_clock::time_point batch_start_{};
    };

    std::string build_request_block(const Options& opt) {
        std::string one;
        one += "GET " + opt.path + " HTTP/1.1\r\n";
        one += "Host: " + opt.host + ":" + opt.port + "\r\n";
        one += "User-Agent: demiplane-bomber/2.0\r\n";
        one += "Accept: */*\r\n";
        one += "\r\n";  // keep-alive is the HTTP/1.1 default; no Connection header
        std::string block;
        block.reserve(one.size() * opt.pipeline);
        for (std::size_t i = 0; i < opt.pipeline; ++i)
            block += one;
        return block;
    }

    [[noreturn]] void usage(const char* argv0) {
        std::cerr << "Usage: " << argv0
                  << " [--host H] [--port P] [--path /ping] [--threads N] [--conns N]\n"
                     "       [--pipeline N] [--requests N] [--warmup N] [--strict] [--json]\n";
        std::exit(2);
    }

    Options parse(const int argc, char** argv) {
        Options o;
        auto need = [&](const int i) {
            if (i + 1 >= argc)
                usage(argv[0]);
            return std::string_view{argv[i + 1]};
        };
        for (int i = 1; i < argc; ++i) {
            const std::string_view a{argv[i]};
            if (a == "--host") {
                o.host = need(i);
                ++i;
            } else if (a == "--port") {
                o.port = need(i);
                ++i;
            } else if (a == "--path") {
                o.path = need(i);
                ++i;
            } else if (a == "--threads") {
                o.threads = std::stoull(std::string{need(i)});
                ++i;
            } else if (a == "--conns") {
                o.conns = std::stoull(std::string{need(i)});
                ++i;
            } else if (a == "--pipeline") {
                o.pipeline = std::stoull(std::string{need(i)});
                ++i;
            } else if (a == "--requests") {
                o.requests = std::stoull(std::string{need(i)});
                ++i;
            } else if (a == "--warmup") {
                o.warmup = std::stoull(std::string{need(i)});
                ++i;
            } else if (a == "--strict") {
                o.strict = true;
            } else if (a == "--json") {
                o.json = true;
            } else {
                usage(argv[0]);
            }
        }
        if (o.threads == 0 || o.conns == 0 || o.pipeline == 0)
            usage(argv[0]);
        if (o.conns < o.threads)
            o.conns = o.threads;
        return o;
    }

    std::uint64_t percentile(std::span<const std::uint64_t> hist, const std::uint64_t total, const double p) {
        if (total == 0)
            return 0;
        const auto target  = static_cast<std::uint64_t>(static_cast<double>(total) * p);
        std::uint64_t seen = 0;
        for (std::size_t i = 0; i < hist.size(); ++i) {
            seen += hist[i];
            if (seen >= target)
                return bucket_value(i);
        }
        return bucket_value(hist.size() - 1);
    }

}  // namespace

int main(const int argc, char** argv) {
    try {
        const Options opt               = parse(argc, argv);
        const std::string request_block = build_request_block(opt);

        RunState run;
        std::vector<ThreadStats> stats(opt.threads);
        std::vector<std::jthread> workers;
        workers.reserve(opt.threads);

        const std::size_t conns_per_thread = opt.conns / opt.threads;

        if (!opt.json) {
            std::cerr << "bomber: http://" << opt.host << ":" << opt.port << opt.path << "  threads=" << opt.threads
                      << " conns=" << opt.conns << " pipeline=" << opt.pipeline << " warmup=" << opt.warmup
                      << " requests=" << opt.requests << (opt.strict ? " [strict]" : "") << "\n";
        }

        for (std::size_t t = 0; t < opt.threads; ++t) {
            workers.emplace_back([&, t] {
                net::io_context ioc{1};
                tcp::resolver resolver{ioc};
                const auto endpoints = resolver.resolve(opt.host, opt.port);

                std::vector<std::shared_ptr<Connection>> conns;
                conns.reserve(conns_per_thread);
                for (std::size_t c = 0; c < conns_per_thread; ++c) {
                    conns.push_back(std::make_shared<Connection>(ioc,
                                                                 endpoints,
                                                                 opt,
                                                                 stats[t],
                                                                 run,
                                                                 request_block,
                                                                 static_cast<unsigned>(t * conns_per_thread + c)));
                    conns.back()->start();
                }
                ioc.run();
            });
        }

        // Wait for the measuring flag, then time the measured window.
        while (!run.measuring.load(std::memory_order_acquire) && !run.stop.load(std::memory_order_acquire))
            std::this_thread::yield();
        const auto t0 = std::chrono::steady_clock::now();

        for (auto& w : workers)
            w.join();
        const auto t1 = std::chrono::steady_clock::now();

        // ── Aggregate ────────────────────────────────────────────────────────
        std::uint64_t ok = 0, failed = 0, bytes = 0;
        std::vector<std::uint64_t> hist(kBuckets, 0);
        for (const auto& s : stats) {
            ok     += s.ok;
            failed += s.failed;
            bytes  += s.bytes;
            for (std::size_t i = 0; i < kBuckets; ++i)
                hist[i] += s.hist[i];
        }
        // `ok` includes warmup; the measured count is what was published.
        const std::uint64_t measured = run.published.load(std::memory_order_relaxed);
        const double secs            = std::chrono::duration<double>(t1 - t0).count();
        const double rps             = secs > 0 ? static_cast<double>(measured) / secs : 0.0;

        const std::uint64_t samples = std::accumulate(hist.begin(), hist.end(), std::uint64_t{0});
        const auto p50              = percentile(hist, samples, 0.50);
        const auto p99              = percentile(hist, samples, 0.99);
        const auto p999             = percentile(hist, samples, 0.999);

        if (opt.json) {
            std::cout << "{\"requests\":" << measured << ",\"total_incl_warmup\":" << ok << ",\"failed\":" << failed
                      << ",\"seconds\":" << std::fixed << std::setprecision(4) << secs
                      << ",\"rps\":" << std::setprecision(1) << rps << ",\"bytes\":" << bytes << ",\"p50_us\":" << p50
                      << ",\"p99_us\":" << p99 << ",\"p999_us\":" << p999 << ",\"threads\":" << opt.threads
                      << ",\"conns\":" << opt.conns << ",\"pipeline\":" << opt.pipeline << "}\n";
        } else {
            std::cout << "\n=== RESULT ===\n"
                      << "measured requests : " << measured << "\n"
                      << "total incl warmup : " << ok << "\n"
                      << "failed            : " << failed << "\n"
                      << "elapsed           : " << std::fixed << std::setprecision(3) << secs << " s\n"
                      << "throughput        : " << std::setprecision(0) << rps << " req/s\n"
                      << "latency p50/p99/p99.9: " << p50 << " / " << p99 << " / " << p999 << " us\n";
        }
        return failed == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "bomber: " << e.what() << '\n';
        return 1;
    }
}
