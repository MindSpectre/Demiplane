#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;

class HttpBomber {
public:
    HttpBomber(std::string host, std::string port, std::string target, const uint32_t thread_count,
        const uint32_t interval_ms, const uint32_t duration_seconds)
        : host_(std::move(host)), port_(std::move(port)), target_(std::move(target)), thread_count_(thread_count),
          interval_ms_(interval_ms), duration_seconds_(duration_seconds) {}

    void start() {
        std::cout << "Starting HTTP Bomber with " << thread_count_ << " threads\n";
        std::cout << "Target: http://" << host_ << ":" << port_ << target_ << "\n";
        std::cout << "Interval: " << interval_ms_ << "ms\n";
        std::cout << "Duration: " << duration_seconds_ << "s\n";
        std::cout << "Expected requests per second: " << (thread_count_ * 1000) / interval_ms_ << "\n\n";

        running_ = true;

        // Start worker threads
        for (uint32_t i = 0; i < thread_count_; ++i) {
            threads_.emplace_back(&HttpBomber::worker_thread, this, i);
        }

        // Start statistics reporter
        std::thread stats_thread(&HttpBomber::stats_reporter, this);

        // Wait for duration
        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds_));

        // Stop all threads
        running_ = false;

        // Wait for all threads to finish
        for (auto& thread : threads_) {
            thread.join();
        }
        stats_thread.join();

        print_final_stats();
    }

private:
    void worker_thread(uint32_t thread_id) {
        net::io_context ioc;

        // Pre-resolve the address to avoid DNS lookup overhead
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host_, port_);

        while (running_) {
            auto request_start = std::chrono::high_resolution_clock::now();

            try {
                // Create a new connection for each request to simulate real-world usage
                beast::tcp_stream stream(ioc);

                // Time the actual request/response cycle
                // auto connect_start = std::chrono::high_resolution_clock::now();
                stream.connect(results);

                // Set up HTTP GET request
                http::request<http::string_body> req{http::verb::get, target_, 11};
                req.set(http::field::host, host_);
                req.set(http::field::user_agent, "HttpBomber/1.0");
                req.set(http::field::connection, "close"); // Force connection close

                // Send the request and measure response time
                auto send_start = std::chrono::high_resolution_clock::now();
                http::write(stream, req);

                // Read the response
                beast::flat_buffer buffer;
                http::response<http::string_body> res;
                http::read(stream, buffer, res);

                auto response_end = std::chrono::high_resolution_clock::now();

                // Calculate response time (from request sent to response received)
                std::chrono::duration<uint64_t, std::ratio<1, 1000000>> response_time =
                    std::chrono::duration_cast<std::chrono::microseconds>(response_end - send_start);

                // Close the connection
                beast::error_code ec;
                std::ignore = stream.socket().shutdown(tcp::socket::shutdown_both, ec);

                // Check if response indicates success or failure
                if (res.result() == http::status::ok) {
                    ++stats_.successful_requests;
                } else {
                    ++stats_.failed_requests;
                }

                // Update response time statistics (convert to milliseconds with precision)
                uint64_t response_time_ms = response_time.count() / 1000; // Convert microseconds to milliseconds
                stats_.update_response_time(response_time_ms);

            } catch (const std::exception& e) {
                ++stats_.failed_requests;
                // Don't spam error messages unless it's a significant failure rate
                if (stats_.failed_requests.load() % 100 == 1) {
                    std::cerr << "Thread " << thread_id << " error: " << e.what() << std::endl;
                }
            }

            ++stats_.total_requests;

            // Sleep for interval (if still running)
            if (running_) {
                auto request_end = std::chrono::high_resolution_clock::now();

                // Only sleep if we have time left in the interval
                if (auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(request_end - request_start);
                    elapsed.count() < interval_ms_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_ - elapsed.count()));
                }
            }
        }
    }

    void stats_reporter() const {
        const auto start_time = std::chrono::steady_clock::now();

        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Update every 500ms

            auto current_time = std::chrono::steady_clock::now();
            auto elapsed      = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);

            print_current_stats(static_cast<double>(elapsed.count()));
        }
    }

    void print_current_stats(const double elapsed_seconds) const {
        const auto total      = static_cast<double>(stats_.total_requests.load());
        const auto successful = static_cast<double>(stats_.successful_requests.load());
        const auto failed     = static_cast<double>(stats_.failed_requests.load());

        if (total > 0) {
            const double success_rate = (successful * 100.0) / total;
            const double failure_rate = (failed * 100.0) / total;
            const double rps          = elapsed_seconds > 0 ? static_cast<double>(total) / elapsed_seconds : 0;

            std::cout << "\r[" << std::setw(3) << elapsed_seconds << "s] " << std::setw(6) << total << " req | "
                      << std::setw(6) << successful << " ok (" << std::fixed << std::setprecision(1) << success_rate
                      << "%) | " << std::setw(6) << failed << " fail (" << failure_rate << "%) | " << std::setw(6)
                      << std::setprecision(1) << rps << " req/s";

            if (successful > 0) {
                const double avg_response_time = static_cast<double>(stats_.total_response_time_ms.load()) / successful;
                const auto min_response_time   = stats_.min_response_time_ms.load();
                const auto max_response_time   = stats_.max_response_time_ms.load();

                std::cout << " | Avg: " << std::setw(4) << avg_response_time << "ms"
                          << " | Min: " << std::setw(4) << min_response_time << "ms"
                          << " | Max: " << std::setw(4) << max_response_time << "ms";
            }

            std::cout << std::flush;
        }
    }

    void print_final_stats() const {
        std::cout << "\n\n=== FINAL STATISTICS ===\n";

        const auto total      = static_cast<double>(stats_.total_requests.load());
        const auto successful = static_cast<double>(stats_.successful_requests.load());
        const auto failed     = static_cast<double>(stats_.failed_requests.load());

        std::cout << "Total Requests: " << total << "\n";
        std::cout << "Successful: " << successful << " (" << std::fixed << std::setprecision(2)
                  << (successful * 100.0 / total) << "%)\n";
        std::cout << "Failed: " << failed << " (" << (failed * 100.0 / total) << "%)\n";

        if (successful > 0) {
            const auto avg_response_time = static_cast<double>(stats_.total_response_time_ms.load()) / successful;
            const auto min_response_time = stats_.min_response_time_ms.load();
            const auto max_response_time = stats_.max_response_time_ms.load();

            std::cout << "Average Response Time: " << avg_response_time << "ms\n";
            std::cout << "Min Response Time: " << min_response_time << "ms\n";
            std::cout << "Max Response Time: " << max_response_time << "ms\n";
        }

        const double requests_per_second = static_cast<double>(total) / duration_seconds_;
        std::cout << "Actual Requests per Second: " << std::fixed << std::setprecision(2) << requests_per_second
                  << "\n";
        std::cout << "======================\n";
    }

    // -------------------------- Private Members -------------------------- //
    struct Statistics {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> successful_requests{0};
        std::atomic<uint64_t> failed_requests{0};
        std::atomic<uint64_t> total_response_time_ms{0};
        std::atomic<uint64_t> min_response_time_ms{UINT64_MAX};
        std::atomic<uint64_t> max_response_time_ms{0};

        void update_response_time(const uint64_t response_time_ms) {
            total_response_time_ms += response_time_ms;

            uint64_t current_min = min_response_time_ms.load();
            while (response_time_ms < current_min
                   && !min_response_time_ms.compare_exchange_weak(current_min, response_time_ms)) {
            }

            uint64_t current_max = max_response_time_ms.load();
            while (response_time_ms > current_max
                   && !max_response_time_ms.compare_exchange_weak(current_max, response_time_ms)) {
            }
        }
    };

    std::string host_;
    std::string port_;
    std::string target_;
    uint32_t thread_count_;
    uint32_t interval_ms_;
    uint32_t duration_seconds_;
    std::atomic<bool> running_{false};
    Statistics stats_;
    std::vector<std::thread> threads_;
};

int main(const int argc, char* argv[]) {
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <target> <threads> <interval_ms> <duration_seconds>\n";
        std::cerr << "Example: " << argv[0] << " 127.0.0.1 8080 /users/1 4 30 30\n";
        return 1;
    }

    try {
        const std::string host          = argv[1];
        const std::string port          = argv[2];
        const std::string target        = argv[3];
        const uint32_t threads          = static_cast<uint32_t>(std::stoi(argv[4]));
        const uint32_t interval_ms      = static_cast<uint32_t>(std::stoi(argv[5]));
        const uint32_t duration_seconds = static_cast<uint32_t>(std::stoi(argv[6]));

        HttpBomber bomber(host, port, target, threads, interval_ms, duration_seconds);
        bomber.start();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
