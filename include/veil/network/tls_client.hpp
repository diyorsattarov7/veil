#ifndef VEIL_TLS_CLIENT_HPP
#define VEIL_TLS_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>

namespace veil::network {

class TlsClient {
public:
    using tcp = boost::asio::ip::tcp;

    explicit TlsClient(boost::asio::io_context& io);

    void connect(const std::string& host, unsigned short port);
    
    std::string get(const std::string& host, const std::string& target);

private:
    boost::asio::ssl::context ssl_context_;
    tcp::resolver resolver_;
    boost::asio::ssl::stream<tcp::socket> stream_;
};

}

#endif // VEIL_TLS_CLIENT_HPP