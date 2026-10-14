#ifndef VEIL_PROXY_SESSION_HPP
#define VEIL_PROXY_SESSION_HPP

#include <veil/xmpp/stream_parser.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <array>
#include <memory>
#include <string>

namespace veil::network
{

class proxy_session : public std::enable_shared_from_this<proxy_session>
{
public:
	using tcp = boost::asio::ip::tcp;
	using ssl_stream = boost::asio::ssl::stream<tcp::socket>;

	proxy_session(tcp::socket client,
		      boost::asio::ssl::context &server_context,
		      std::string upstream_host,
		      unsigned short upstream_port,
		      std::string ca_file);

	void start();

private:
	void connect_upstream();
	void handshake_client();
	void handshake_upstream();

	void read_client();
	void read_upstream();

	void finish_client();
	void finish_upstream();
	void stop();

	ssl_stream client_;
	boost::asio::ssl::context upstream_context_;
	ssl_stream upstream_;
	tcp::resolver resolver_;

	std::string upstream_host_;
	unsigned short upstream_port_;

	std::array<char, 8192> client_buffer_{};
	std::array<char, 8192> upstream_buffer_{};

	bool client_finished_ = false;
	bool upstream_finished_ = false;
	bool stopped_ = false;

	veil::xmpp::stream_parser xmpp_parser_;
	std::string pending_client_write_;
};

} // namespace veil::network

#endif // VEIL_PROXY_SESSION_HPP