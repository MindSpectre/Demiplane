#include <span>
#include <string_view>

#include <connection_concepts.hpp>
#include <gtest/gtest.h>
#include <http2_driver.hpp>
#include <http3_driver.hpp>
#include <http_driver_concept.hpp>
#include <http_enums.hpp>
#include <quic_connection.hpp>

using namespace demiplane::http;

namespace {
    struct GoodDriver {
        static constexpr Protocol id() {
            return Protocol::http1;
        }
        static constexpr std::span<const std::string_view> accepted_alpns() {
            static constexpr std::string_view kAlpns[] = {"http/1.1"};
            return kAlpns;
        }
    };
    struct MissingAlpns {
        static constexpr Protocol id() {
            return Protocol::http1;
        }
    };
}  // namespace

static_assert(HttpDriver<GoodDriver>);
static_assert(!HttpDriver<MissingAlpns>);

TEST(HttpDriverConceptTest, AdvertisesIdAndAlpns) {
    EXPECT_EQ(GoodDriver::id(), Protocol::http1);
    ASSERT_EQ(GoodDriver::accepted_alpns().size(), 1u);
    EXPECT_EQ(GoodDriver::accepted_alpns()[0], "http/1.1");
}

static_assert(HttpDriver<Http2Driver>);
static_assert(HttpDriver<Http3Driver>);
static_assert(Connection<QuicConnection> && !StreamConnection<QuicConnection>);

TEST(DriverScaffoldsTest, Http2AdvertisesH2) {
    EXPECT_EQ(Http2Driver::id(), Protocol::http2);
    ASSERT_EQ(Http2Driver::accepted_alpns().size(), 1u);
    EXPECT_EQ(Http2Driver::accepted_alpns()[0], "h2");
}

TEST(DriverScaffoldsTest, Http3AdvertisesH3) {
    EXPECT_EQ(Http3Driver::id(), Protocol::http3);
    ASSERT_EQ(Http3Driver::accepted_alpns().size(), 1u);
    EXPECT_EQ(Http3Driver::accepted_alpns()[0], "h3");
}
