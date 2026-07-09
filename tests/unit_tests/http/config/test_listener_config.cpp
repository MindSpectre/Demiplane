#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include <http_enums.hpp>
#include <json/json.hpp>
#include <listener_config.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;

namespace {
    TlsConfig valid_tls() {
        return TlsConfig::Builder{}.cert_file("c.pem").key_file("k.pem").finalize();
    }
}  // namespace

TEST(ListenerConfigTest, DefaultsMatchSpec) {
    const auto l = ListenerConfig::deserialize<Json::Value>(Json::Value{Json::objectValue});
    EXPECT_EQ(l.bind_address(), "0.0.0.0");
    EXPECT_EQ(l.port(), 8080);
    EXPECT_EQ(l.transport(), ListenerConfig::Transport::tcp);
    EXPECT_TRUE(l.protocols().empty());
    EXPECT_FALSE(l.tls().has_value());
    EXPECT_EQ(l.effective_protocols(), std::vector{Protocol::http1});
}

TEST(ListenerConfigTest, RoundTripPreservesProtocolOrder) {
    const auto l = ListenerConfig::Builder{}
                       .bind_address("127.0.0.1")
                       .port(8443)
                       .transport(ListenerConfig::Transport::tls)
                       .protocols({Protocol::http2, Protocol::http1})
                       .tls(valid_tls())
                       .finalize();
    const auto json = l.serialize<Json::Value>();
    EXPECT_EQ(json["transport"].asString(), "tls");
    ASSERT_TRUE(json["protocols"].isArray());
    EXPECT_EQ(json["protocols"][0].asString(), "http2");
    EXPECT_EQ(json["protocols"][1].asString(), "http1");
    const auto back = ListenerConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(back.bind_address(), "127.0.0.1");
    EXPECT_EQ(back.port(), 8443);
    EXPECT_EQ(back.transport(), ListenerConfig::Transport::tls);
    EXPECT_EQ(back.effective_protocols(), (std::vector{Protocol::http2, Protocol::http1}));
    ASSERT_TRUE(back.tls().has_value());
    EXPECT_EQ(back.tls()->cert_file(), "c.pem");
}

TEST(ListenerConfigTest, UnknownTransportOrProtocolStringThrows) {
    Json::Value bad_transport{Json::objectValue};
    bad_transport["transport"] = "udp";
    EXPECT_THROW((void)ListenerConfig::deserialize<Json::Value>(bad_transport), std::invalid_argument);

    Json::Value bad_protocol{Json::objectValue};
    bad_protocol["protocols"] = Json::Value{Json::arrayValue};
    bad_protocol["protocols"].append("gopher");
    EXPECT_THROW((void)ListenerConfig::deserialize<Json::Value>(bad_protocol), std::invalid_argument);
}

TEST(ListenerConfigTest, ValidateEnforcesProtocolFacts) {
    // http3 off quic
    EXPECT_THROW((void)ListenerConfig::Builder{}.protocols({Protocol::http3}).finalize(), std::invalid_argument);
    // quic with a non-h3 set
    EXPECT_THROW((void)ListenerConfig::Builder{}
                     .transport(ListenerConfig::Transport::quic)
                     .protocols({Protocol::http1})
                     .tls(valid_tls())
                     .finalize(),
                 std::invalid_argument);
    // duplicates
    EXPECT_THROW((void)ListenerConfig::Builder{}.protocols({Protocol::http1, Protocol::http1}).finalize(),
                 std::invalid_argument);
    // tls transport without material
    EXPECT_THROW((void)ListenerConfig::Builder{}.transport(ListenerConfig::Transport::tls).finalize(),
                 std::invalid_argument);
    // tcp WITH material
    EXPECT_THROW((void)ListenerConfig::Builder{}.tls(valid_tls()).finalize(), std::invalid_argument);
    // nested TlsConfig is validated
    EXPECT_THROW((void)ListenerConfig::Builder{}
                     .transport(ListenerConfig::Transport::tls)
                     .tls(TlsConfig{"", ""})
                     .finalize(),
                 std::invalid_argument);
}

TEST(ListenerConfigTest, QuicDefaultsToH3) {
    const auto l = ListenerConfig::Builder{}
                       .transport(ListenerConfig::Transport::quic)
                       .tls(valid_tls())
                       .finalize();
    EXPECT_EQ(l.effective_protocols(), std::vector{Protocol::http3});
}
