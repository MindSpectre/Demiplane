#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

#include <body.hpp>
#include <http_enums.hpp>
#include <json/json.hpp>
#include <listener_config.hpp>
#include <load_server_config.hpp>
#include <response.hpp>
#include <server_config.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;
using namespace std::chrono_literals;

namespace {

    std::string write_temp(const std::string_view stem, const std::string_view contents) {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("dmp_http_cfg_" + std::to_string(::getpid()) + "_" + std::string{stem});
        std::ofstream out{path, std::ios::binary | std::ios::trunc};
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return path.string();
    }

    std::string body_of(const Response& r) {
        return std::string{r.body.buffered_view().value_or("")};
    }

}  // namespace

TEST(LoadServerConfigTest, LoadsValidConfig) {
    const auto path = write_temp("valid.json", R"({
        "threads": 4,
        "body_limit": 65536,
        "request_arena_size": 16384,
        "drain_timeout_ms": 5000,
        "path_normalization": "collapse_multi_slash",
        "timeouts": { "header_ms": 5000, "body_ms": 10000, "idle_ms": 30000 },
        "listeners": [
            { "bind": "127.0.0.1", "port": 8080, "transport": "tcp", "protocols": ["http1"] },
            { "bind": "127.0.0.1", "port": 8443, "transport": "tls", "protocols": ["http2", "http1"],
              "tls": { "cert_file": "c.pem", "key_file": "k.pem", "min_version": "tls13" } }
        ]
    })");
    auto outcome = load_server_config(path);
    ASSERT_TRUE(outcome.is_success());
    const auto& cfg = outcome.value();
    EXPECT_EQ(cfg.threads(), 4u);
    EXPECT_EQ(cfg.body_limit(), 65536u);
    EXPECT_EQ(cfg.request_arena_size(), 16384u);
    EXPECT_EQ(cfg.drain_timeout(), 5000ms);
    EXPECT_EQ(cfg.path_normalization(), ServerConfig::PathNormalization::collapse_multi_slash);
    EXPECT_EQ(cfg.timeouts().header(), 5000ms);
    EXPECT_EQ(cfg.timeouts().body(), 10000ms);
    EXPECT_EQ(cfg.timeouts().idle(), 30000ms);
    ASSERT_EQ(cfg.listeners().size(), 2u);
    EXPECT_EQ(cfg.listeners()[0].transport(), ListenerConfig::Transport::tcp);
    EXPECT_EQ(cfg.listeners()[0].port(), 8080);
    const auto& tls_listener = cfg.listeners()[1];
    EXPECT_EQ(tls_listener.effective_protocols(), (std::vector{Protocol::http2, Protocol::http1}));
    ASSERT_TRUE(tls_listener.tls().has_value());
    EXPECT_EQ(tls_listener.tls()->min_version(), TlsConfig::MinVersion::tls13);
}

TEST(LoadServerConfigTest, MissingFileIsFileError) {
    auto outcome = load_server_config("/nonexistent/dir/server.json");
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigFileError>());
    EXPECT_EQ(outcome.error<ConfigFileError>().path, "/nonexistent/dir/server.json");
}

TEST(LoadServerConfigTest, DirectoryIsFileError) {
    auto outcome = load_server_config(std::filesystem::temp_directory_path().string());
    ASSERT_TRUE(outcome.is_error());
    EXPECT_TRUE(outcome.holds_error<ConfigFileError>());
}

TEST(LoadServerConfigTest, MalformedJsonIsParseErrorWithLine) {
    const auto path = write_temp("malformed.json", R"({"threads": })");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigParseError>());
    const auto& err = outcome.error<ConfigParseError>();
    EXPECT_EQ(err.line, 1u);
    EXPECT_FALSE(err.detail.empty());
}

TEST(LoadServerConfigTest, NonObjectTopLevelIsSchemaError) {
    const auto path = write_temp("toplevel.json", "42");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    EXPECT_TRUE(outcome.holds_error<ConfigSchemaError>());
}

TEST(LoadServerConfigTest, TypeMismatchIsSchemaError) {
    const auto path = write_temp("mismatch.json", R"({"threads": "four"})");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigSchemaError>());
    EXPECT_NE(outcome.error<ConfigSchemaError>().detail.find("type mismatch"), std::string::npos);
}

TEST(LoadServerConfigTest, UnknownEnumStringIsSchemaErrorNamingField) {
    const auto path = write_temp("enum.json", R"({"path_normalization": "collapse_everything"})");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigSchemaError>());
    EXPECT_NE(outcome.error<ConfigSchemaError>().detail.find("path_normalization"), std::string::npos);
}

TEST(LoadServerConfigTest, ValidateFailureIsSchemaErrorNamingField) {
    const auto path = write_temp("novalidate.json", R"({"listeners": [{"transport": "tls"}]})");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigSchemaError>());
    EXPECT_NE(outcome.error<ConfigSchemaError>().detail.find("listener.tls"), std::string::npos);
}

TEST(LoadServerConfigTest, DumpLoadRoundTripIsAFixedPoint) {
    const auto cfg = ServerConfig::Builder{}
                         .threads(2)
                         .add_listener(ListenerConfig::Builder{}.bind_address("127.0.0.1").port(9090).finalize())
                         .finalize();
    const Json::Value dumped = dump_server_config(cfg);
    const auto path          = write_temp("roundtrip.json", dumped.toStyledString());
    auto outcome             = load_server_config(path);
    ASSERT_TRUE(outcome.is_success());
    EXPECT_EQ(dump_server_config(outcome.value()), dumped);
}

TEST(LoadServerConfigTest, ErrorsRenderAs500) {
    const auto file = to_http_response(ConfigFileError{"/etc/x.json", "cannot open"});
    EXPECT_EQ(file.status, HttpStatus::internal_server_error);
    EXPECT_NE(body_of(file).find("/etc/x.json"), std::string::npos);

    const auto parse = to_http_response(ConfigParseError{"/etc/x.json", 3, "syntax"});
    EXPECT_EQ(parse.status, HttpStatus::internal_server_error);
    EXPECT_NE(body_of(parse).find("line 3"), std::string::npos);

    const auto schema = to_http_response(ConfigSchemaError{"/etc/x.json", "", "threads must be >= 1"});
    EXPECT_EQ(schema.status, HttpStatus::internal_server_error);
    EXPECT_NE(body_of(schema).find("threads"), std::string::npos);
}
