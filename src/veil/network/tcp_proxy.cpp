#include <veil/network/tcp_proxy.hpp>
#include <veil/network/proxy_session.hpp>

#include <iostream>
#include <memory>
#include <utility>

namespace veil::network
{

namespace asio = boost::asio;

tcp_proxy::tcp_proxy(asio::io_context &io,
		     unsigned short port,
		     asio::ssl::context &server_context,
		     std::string upstream_host,
		     unsigned short upstream_port)
    : acceptor_(io, tcp::endpoint(asio::ip::address_v4::loopback(), port))
    , server_context_(server_context)
    , upstream_host_(std::move(upstream_host))
    , upstream_port_(upstream_port)
{
}

void tcp_proxy::start()
{
	accept();
}

void tcp_proxy::accept()
{
	acceptor_.async_accept([this](boost::system::error_code ec,
				      tcp::socket socket) {
		if (!ec) {
			std::cout << "[CHAT] Client connected\n";

			std::make_shared<proxy_session>(std::move(socket),
							server_context_,
							upstream_host_,
							upstream_port_)
				->start();
		} else if (ec != asio::error::operation_aborted) {
			std::cerr << "[CHAT] Accept: " << ec.message() << '\n';
		}

		if (acceptor_.is_open()) {
			accept();
		}
	});
}

} // namespace veil::network