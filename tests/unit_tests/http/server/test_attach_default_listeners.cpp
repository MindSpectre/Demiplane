#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/asio/io_context.hpp>

#include <attach_default_listeners.hpp>
#include <http_enums.hpp>
#include <listener_config.hpp>
#include <server.hpp>
#include <server_config.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;

namespace {

    TlsConfig dummy_tls() {
        // Paths need not exist: bind() (which builds the SSL ctx) never runs here.
        return TlsConfig::Builder{}.cert_file("c.pem").key_file("k.pem").finalize();
    }

    ServerConfig cfg_with(std::vector<ListenerConfig> listeners) {
        return ServerConfig::Builder{}.listeners(std::move(listeners)).finalize();
    }

    ListenerConfig tls_listener(std::vector<Protocol> protocols) {
        return ListenerConfig::Builder{}
            .bind_address("127.0.0.1")
            .port(0)
            .transport(ListenerConfig::Transport::tls)
            .protocols(std::move(protocols))
            .tls(dummy_tls())
            .finalize();
    }

}  // namespace

TEST(AttachDefaultListenersTest, EmptyConfigIsANoOp) {
    boost::asio::io_context ioc;
    Server server{ServerConfig::Builder{}.finalize(), ioc.get_executor()};
    attach_default_listeners(server);
    EXPECT_TRUE(server.listeners().empty());
}

TEST(AttachDefaultListenersTest, AttachesTcpH1WithEmptyProtocolsDefault) {
    boost::asio::io_context ioc;
    // No protocols listed — effective_protocols() defaults to [http1].
    Server server{
        cfg_with({ListenerConfig::Builder{}.bind_address("127.0.0.1").port(0).finalize()}), ioc.get_executor()};
    attach_default_listeners(server);
    ASSERT_EQ(server.listeners().size(), 1u);
    EXPECT_EQ(server.listeners().front()->bind_address(), "127.0.0.1");
}

TEST(AttachDefaultListenersTest, AttachesEveryTlsCombo) {
    boost::asio::io_context ioc;
    Server server{cfg_with({tls_listener({Protocol::http1}),
                            tls_listener({Protocol::http2}),
                            tls_listener({Protocol::http1, Protocol::http2}),
                            tls_listener({Protocol::http2, Protocol::http1})}),
                  ioc.get_executor()};
    attach_default_listeners(server);
    EXPECT_EQ(server.listeners().size(), 4u);
}

TEST(AttachDefaultListenersTest, AttachesQuicH3Scaffold) {
    boost::asio::io_context ioc;
    Server server{cfg_with({ListenerConfig::Builder{}
                                .bind_address("127.0.0.1")
                                .port(0)
                                .transport(ListenerConfig::Transport::quic)
                                .tls(dummy_tls())
                                .finalize()}),
                  ioc.get_executor()};
    attach_default_listeners(server);
    EXPECT_EQ(server.listeners().size(), 1u);
}

TEST(AttachDefaultListenersTest, RejectsH2cOverTcp) {
    // tcp+[http2] passes ListenerConfig::validate() (h2c is a protocol fact)
    // but v1 has no cleartext-h2 path — attach throws (D5).
    boost::asio::io_context ioc;
    Server server{
        cfg_with({ListenerConfig::Builder{}.bind_address("127.0.0.1").port(0).protocols({Protocol::http2}).finalize()}),
        ioc.get_executor()};
    EXPECT_THROW(attach_default_listeners(server), std::invalid_argument);
}
