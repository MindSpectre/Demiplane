#pragma once

#include <chrono>
#include <memory>
#include <memory_resource>
#include <string>
#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <connection_concepts.hpp>
#include <request_arena.hpp>
#include <router.hpp>

namespace http_driver_test {

    /// IsStreamConnection over an in-memory beast::test::stream — no kernel
    /// sockets. set_deadline_after is a no-op (no watchdog runs here; deadline
    /// enforcement is the listener's deadline_watchdog, integration-tested).
    class TestConnection {
    public:
        using stream_type = boost::beast::test::stream;

        explicit TestConnection(boost::asio::io_context& ioc, std::size_t arena_size = 8192)
            : stream_{ioc.get_executor()},
              arena_{arena_size} {
        }

        stream_type& stream() noexcept {
            return stream_;
        }
        std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }
        void reset_request_arena() {
            arena_.reset();
        }
        void set_deadline_after(std::chrono::milliseconds) noexcept {
        }
        boost::asio::awaitable<void> async_close() {
            stream_.close();
            co_return;
        }
        boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }
        [[nodiscard]] boost::asio::ip::address remote_address() const {
            demiplane::gears::force_non_static(this);
            return boost::asio::ip::make_address("127.0.0.1");
        }
        [[nodiscard]] static demiplane::http::Protocol negotiated_protocol() noexcept {
            return demiplane::http::Protocol::http1;
        }
        [[nodiscard]] static bool is_secure() noexcept {
            return false;
        }

    private:
        stream_type stream_;
        demiplane::http::RequestArena arena_;
        boost::asio::cancellation_signal signal_;
    };

    static_assert(demiplane::http::IsStreamConnection<TestConnection>);

    using ParsedResponse = boost::beast::http::response<boost::beast::http::string_body>;

    /// Drive `driver.serve(conn, router)` against an in-memory peer that writes
    /// `requests` then reads `expected` responses. Returns the parsed responses.
    template <typename Driver>
    std::vector<ParsedResponse> exchange(Driver& driver,
                                         demiplane::http::Router& router,
                                         const std::vector<std::string>& requests,
                                         const int expected) {
        namespace asio  = boost::asio;
        namespace beast = boost::beast;
        namespace http  = beast::http;

        asio::io_context ioc;
        TestConnection conn{ioc};
        beast::test::stream client{ioc.get_executor()};
        conn.stream().connect(client);

        std::vector<ParsedResponse> responses;

        asio::co_spawn(ioc, driver.serve(conn, router), asio::detached);
        asio::co_spawn(
            ioc,
            [&]() -> asio::awaitable<void> {
                for (const auto& req : requests)
                    co_await asio::async_write(client, asio::buffer(req), asio::use_awaitable);
                beast::flat_buffer buffer;
                beast::error_code ec;
                for (int i = 0; i < expected; ++i) {
                    ParsedResponse res;
                    co_await http::async_read(client, buffer, res, asio::redirect_error(asio::use_awaitable, ec));
                    if (ec)
                        break;
                    responses.push_back(std::move(res));
                }
                client.close();
                co_return;
            },
            asio::detached);

        ioc.run();
        return responses;
    }

}  // namespace http_driver_test
