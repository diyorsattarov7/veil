#ifndef VEIL_HTTP_SERVER_HPP
#define VEIL_HTTP_SERVER_HPP

#include <boost/asio.hpp>

#include <functional>
#include <string>

namespace veil::network {

class HttpServer {
public:
    using tcp = boost::asio::ip::tcp;

    using ChatServerHandler =
        std::function<bool(std::string, unsigned short)>;

    HttpServer(
        boost::asio::io_context& io,
        unsigned short port,
        std::string chat_host,
        unsigned short chat_port,
        std::string ca_file
    );

    void start();
    void set_chat_server_handler(ChatServerHandler handler);

private:
    void accept();

    tcp::acceptor acceptor_;
    ChatServerHandler chat_server_handler_;
    std::string local_chat_host_;
    unsigned short local_chat_port_;
    std::string ca_file_;
};

}

#endif // VEIL_HTTP_SERVER_HPP