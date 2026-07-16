#include <chrono>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>
#include <http_enums.hpp>
#include <json/json.hpp>
#include <listener_config.hpp>
#include <server_config.hpp>
#include <timeouts.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;
using namespace std::chrono_literals;

TEST(ServerConfigTest, DefaultsMatchSpec) {
    const auto cfg = ServerConfig::Builder{}.finalize();
    EXPECT_TRUE(cfg.listeners().empty());
    EXPECT_EQ(cfg.threads(), 1u);
    EXPECT_EQ(cfg.timeouts().header(), 10s);
    EXPECT_EQ(cfg.timeouts().body(), 30s);
    EXPECT_EQ(cfg.timeouts().idle(), 60s);
    EXPECT_EQ(cfg.body_limit(), 16u * 1024 * 1024);
    EXPECT_EQ(cfg.request_arena_size(), 8192u);
    EXPECT_EQ(cfg.drain_timeout(), 30s);
    EXPECT_EQ(cfg.path_normalization(), ServerConfig::PathNormalization::collapse_trailing_slash);
}

TEST(ServerConfigTest, RoundTripPreservesNestedListeners) {
    const auto cfg = ServerConfig::Builder{}
                         .threads(4)
                         .body_limit(65536)
                         .request_arena_size(16384)
                         .drain_timeout(5s)
                         .path_normalization(ServerConfig::PathNormalization::collapse_multi_slash)
                         .timeouts(Timeouts{5s, 10s, 30s})
                         .add_listener(ListenerConfig::Builder{}.bind_address("127.0.0.1").port(8080).finalize())
                         .add_listener(ListenerConfig::Builder{}
                                           .bind_address("127.0.0.1")
                                           .port(8443)
                                           .transport(ListenerConfig::Transport::tls)
                                           .protocols({Protocol::http1})
                                           .tls(TlsConfig::Builder{}.cert_file("c.pem").key_file("k.pem").finalize())
                                           .finalize())
                         .finalize();

    const auto json = cfg.serialize<Json::Value>();
    EXPECT_EQ(json["path_normalization"].asString(), "collapse_multi_slash");
    ASSERT_TRUE(json["listeners"].isArray());
    ASSERT_EQ(json["listeners"].size(), 2u);

    const auto back = ServerConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(back.threads(), 4u);
    EXPECT_EQ(back.body_limit(), 65536u);
    EXPECT_EQ(back.request_arena_size(), 16384u);
    EXPECT_EQ(back.drain_timeout(), 5s);
    EXPECT_EQ(back.path_normalization(), ServerConfig::PathNormalization::collapse_multi_slash);
    EXPECT_EQ(back.timeouts().header(), 5s);
    ASSERT_EQ(back.listeners().size(), 2u);
    EXPECT_EQ(back.listeners()[0].transport(), ListenerConfig::Transport::tcp);
    EXPECT_EQ(back.listeners()[1].transport(), ListenerConfig::Transport::tls);
    ASSERT_TRUE(back.listeners()[1].tls().has_value());
    EXPECT_EQ(back.listeners()[1].tls()->cert_file(), "c.pem");

    // serialize → deserialize → serialize is a fixed point (Json::Value ==).
    EXPECT_EQ(back.serialize<Json::Value>(), json);
}

TEST(ServerConfigTest, UnknownPathNormalizationStringThrows) {
    Json::Value json{Json::objectValue};
    json["path_normalization"] = "collapse_everything";
    EXPECT_THROW((void)ServerConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(ServerConfigTest, ValidateRejectsBadScalars) {
    EXPECT_THROW((void)ServerConfig::Builder{}.threads(0).finalize(), std::invalid_argument);
    EXPECT_THROW((void)ServerConfig::Builder{}.body_limit(0).finalize(), std::invalid_argument);
    EXPECT_THROW((void)ServerConfig::Builder{}.request_arena_size(0).finalize(), std::invalid_argument);
    EXPECT_THROW((void)ServerConfig::Builder{}.drain_timeout(-1ms).finalize(), std::invalid_argument);
}

TEST(ServerConfigTest, DeserializeRejectsInvalidNestedListener) {
    // tls transport without material: rejected on the loading path (the
    // element's own deserialize→finalize→validate chain).
    Json::Value listener{Json::objectValue};
    listener["transport"] = "tls";
    Json::Value json{Json::objectValue};
    json["listeners"] = Json::Value{Json::arrayValue};
    json["listeners"].append(listener);
    EXPECT_THROW((void)ServerConfig::deserialize<Json::Value>(json), std::invalid_argument);
}
