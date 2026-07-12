#include "tcp_connection.hpp"

#include <boost/beast/core/error.hpp>

namespace demiplane::http {

    boost::asio::awaitable<void, Strand> TcpConnection::async_close() {
        boost::beast::error_code ec;
        stream_.shutdown(boost::asio::ip::tcp::socket::shutdown_send,
                         ec);  // returns void under BOOST_ASIO_NO_DEPRECATED
        // Best-effort half-close; the socket closes when the stream is destroyed.
        co_return;
    }

}  // namespace demiplane::http
