#include <veil/network/proxy_session.hpp>
#include <veil/network/tcp_proxy.hpp>

#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

namespace veil::network
{

namespace asio = boost::asio;

tcp_proxy::tcp_proxy(asio::io_context &io, unsigned short port,
                     asio::ssl::context &server_context,
                     std::string upstream_host, unsigned short upstream_port,
                     std::string ca_file)
    : acceptor_(io, tcp::endpoint(asio::ip::address_v4::loopback(), port)),
      server_context_(server_context), upstream_host_(std::move(upstream_host)),
      upstream_port_(upstream_port), ca_file_(std::move(ca_file))
{
}

void tcp_proxy::start() { accept(); }

void tcp_proxy::accept()
{
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
            auto logger = spdlog::get("chat");

            if (!logger)
            {
                logger = spdlog::default_logger();
            }

            if (!ec)
            {
                boost::system::error_code endpoint_ec;
                const auto endpoint = socket.remote_endpoint(endpoint_ec);

                if (!endpoint_ec)
                {
                    logger->debug("Client connected: {}:{}",
                                  endpoint.address().to_string(),
                                  endpoint.port());
                }
                else
                {
                    logger->debug("Client connected");
                }

                try
                {
                    std::make_shared<proxy_session>(
                        std::move(socket), server_context_, upstream_host_,
                        upstream_port_, ca_file_)
                        ->start();
                }
                catch (const std::exception &e)
                {
                    logger->error("Failed to initialize client session: {}",
                                  e.what());
                }
            }
            else if (ec != asio::error::operation_aborted)
            {
                logger->error("Accept failed: {}", ec.message());
            }

            if (acceptor_.is_open())
            {
                accept();
            }
        });
}

} // namespace veil::network
