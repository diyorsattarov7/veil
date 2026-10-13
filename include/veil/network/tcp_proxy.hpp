#ifndef VEIL_TCP_PROXY_HPP
#define VEIL_TCP_PROXY_HPP

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>

namespace veil::network {

class TcpProxy {
public:
    using tcp = boost::asio::ip::tcp;

    TcpProxy(
        boost::asio::io_context& io,
        unsigned short port,
        boost::asio::ssl::context& server_context,
        std::string upstream_host,
        unsigned short upstream_port
    );

    void start();

private:
    void accept();

    tcp::acceptor acceptor_;
    boost::asio::ssl::context& server_context_;
    std::string upstream_host_;
    unsigned short upstream_port_;
};

}

#endif // VEIL_TCP_PROXY_HPP