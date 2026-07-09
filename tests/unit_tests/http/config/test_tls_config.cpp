#include <gtest/gtest.h>

#include <stdexcept>

#include <json/json.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;

TEST(TlsConfigTest, BuilderRoundTripsThroughJson) {
    const auto cfg = TlsConfig::Builder{}
                         .cert_file("/etc/ssl/cert.pem")
                         .key_file("/etc/ssl/key.pem")
                         .dh_params_file("/etc/ssl/dh.pem")
                         .ca_file("/etc/ssl/ca.pem")
                         .min_version(TlsConfig::MinVersion::tls13)
                         .session_cache(false)
                         .require_client_cert(true)
                         .finalize();
    const auto json = cfg.serialize<Json::Value>();
    EXPECT_EQ(json["min_version"].asString(), "tls13");
    const auto back = TlsConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(back.cert_file(), "/etc/ssl/cert.pem");
    EXPECT_EQ(back.key_file(), "/etc/ssl/key.pem");
    EXPECT_EQ(back.dh_params_file(), "/etc/ssl/dh.pem");
    EXPECT_EQ(back.ca_file(), "/etc/ssl/ca.pem");
    EXPECT_EQ(back.min_version(), TlsConfig::MinVersion::tls13);
    EXPECT_FALSE(back.session_cache());
    EXPECT_TRUE(back.require_client_cert());
}

TEST(TlsConfigTest, PassphraseIsSecretReadButNeverDumped) {
    Json::Value json{Json::objectValue};
    json["cert_file"]      = "c.pem";
    json["key_file"]       = "k.pem";
    json["key_passphrase"] = "hunter2";
    const auto cfg = TlsConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(cfg.key_passphrase(), "hunter2");
    EXPECT_FALSE(cfg.serialize<Json::Value>().isMember("key_passphrase"));
}

TEST(TlsConfigTest, UnknownMinVersionStringThrows) {
    Json::Value json{Json::objectValue};
    json["cert_file"]   = "c.pem";
    json["key_file"]    = "k.pem";
    json["min_version"] = "ssl3";
    EXPECT_THROW((void)TlsConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(TlsConfigTest, ValidateRequiresCertKeyAndCaForClientCerts) {
    EXPECT_THROW((void)TlsConfig::Builder{}.key_file("k.pem").finalize(), std::invalid_argument);
    EXPECT_THROW((void)TlsConfig::Builder{}.cert_file("c.pem").finalize(), std::invalid_argument);
    EXPECT_THROW(
        (void)TlsConfig::Builder{}.cert_file("c.pem").key_file("k.pem").require_client_cert(true).finalize(),
        std::invalid_argument);
}

TEST(TlsConfigTest, FullCtorIsNoValidationEscapeHatch) {
    const TlsConfig empty{"", ""};  // must construct — scaffold tests rely on it (D6)
    EXPECT_TRUE(empty.cert_file().empty());
    EXPECT_THROW((void)empty.serialize<Json::Value>(), std::invalid_argument);  // serialize() validates
}
