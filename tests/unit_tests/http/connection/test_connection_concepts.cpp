#include <chrono>
#include <memory_resource>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <connection_concepts.hpp>
#include <gtest/gtest.h>
#include <http_enums.hpp>

using namespace demiplane::http;
// NOLINTBEGIN
namespace {
    // Minimal in-line model that satisfies StreamConnection. Proves the concept
    // is satisfiable with a plausible shape (TcpConnection is checked in Task 14,
    // TestConnection in Task 9).
    struct FakeStream {};

    struct ModelConn {
        using stream_type = FakeStream;
        FakeStream s;

        std::pmr::polymorphic_allocator<> arena_alloc() {
            return {};
        }
        void reset_request_arena() {
        }
        void expires_after(std::chrono::milliseconds) {
        }
        boost::asio::awaitable<void> async_close() {
            co_return;
        }
        boost::asio::cancellation_slot cancel_slot() {
            return {};
        }
        boost::asio::ip::address remote_address() const {
            return {};
        }
        static Protocol negotiated_protocol() {
            return Protocol::http1;
        }
        static bool is_secure() {
            return false;
        }
        stream_type& stream() {
            return s;
        }
    };

    struct NoStream {  // satisfies Connection but not StreamConnection
        std::pmr::polymorphic_allocator<> arena_alloc() {
            return {};
        }
        void reset_request_arena() {
        }
        void expires_after(std::chrono::milliseconds) {
        }
        boost::asio::awaitable<void> async_close() {
            co_return;
        }
        boost::asio::cancellation_slot cancel_slot() {
            return {};
        }
        boost::asio::ip::address remote_address() const {
            return {};
        }
        static Protocol negotiated_protocol() {
            return Protocol::http1;
        }
        static bool is_secure() {
            return false;
        }
    };
}  // namespace
// NOLINTEND

static_assert(Connection<ModelConn>);
static_assert(StreamConnection<ModelConn>);
static_assert(Connection<NoStream>);
static_assert(!StreamConnection<NoStream>);

TEST(ConnectionConceptsTest, ModelsAreCheckedAtCompileTime) {
    SUCCEED();  // the static_asserts above are the test
}
