#include <veil/network/proxy_session.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace veil::network
{

namespace asio = boost::asio;
using error_code = boost::system::error_code;

proxy_session::proxy_session(tcp::socket client,
			     asio::ssl::context &server_context,
			     std::string upstream_host,
			     unsigned short upstream_port)
    : client_(std::move(client), server_context)
    , upstream_context_(asio::ssl::context::tls_client)
    , upstream_(client_.get_executor(), upstream_context_)
    , resolver_(client_.get_executor())
    , upstream_host_(std::move(upstream_host))
    , upstream_port_(upstream_port)
    , xmpp_parser_(veil::xmpp::stream_parser::Mode::Invisible)
{

	upstream_context_.set_verify_mode(asio::ssl::verify_peer);
	upstream_context_.load_verify_file("certs/cacert.pem");

	upstream_.set_verify_callback(
		asio::ssl::host_name_verification(upstream_host_));

	if (!SSL_set_tlsext_host_name(upstream_.native_handle(),
				      upstream_host_.c_str())) {
		throw std::runtime_error("Unable to configure upstream SNI");
	}
}

void proxy_session::start()
{
	handshake_client();
}

void proxy_session::handshake_client()
{
	auto self = shared_from_this();

	client_.async_handshake(
		asio::ssl::stream_base::server, [self](error_code ec) {
			if (ec) {
				std::cerr
					<< "[CHAT] Client TLS: " << ec.message()
					<< '\n';
				self->stop();
				return;
			}

			std::cout << "[CHAT] Client TLS established\n";
			self->connect_upstream();
		});
}

void proxy_session::connect_upstream()
{
	auto self = shared_from_this();

	resolver_.async_resolve(
		upstream_host_,
		std::to_string(upstream_port_),
		[self](error_code ec, tcp::resolver::results_type endpoints) {
			if (ec) {
				std::cerr << "[CHAT] Resolve: " << ec.message()
					  << '\n';
				self->stop();
				return;
			}

			asio::async_connect(
				self->upstream_.next_layer(),
				endpoints,
				[self](error_code ec, const tcp::endpoint &) {
					if (ec) {
						std::cerr << "[CHAT] Connect: "
							  << ec.message()
							  << '\n';
						self->stop();
						return;
					}

					self->handshake_upstream();
				});
		});
}

void proxy_session::handshake_upstream()
{
	auto self = shared_from_this();

	upstream_.async_handshake(
		asio::ssl::stream_base::client, [self](error_code ec) {
			if (ec) {
				std::cerr << "[CHAT] Upstream TLS: "
					  << ec.message() << '\n';
				self->stop();
				return;
			}

			std::cout << "[CHAT] Upstream TLS established\n";

			self->read_client();
			self->read_upstream();
		});
}

void proxy_session::read_client()
{
	auto self = shared_from_this();

	client_.async_read_some(
		asio::buffer(client_buffer_),
		[self](error_code ec, std::size_t bytes) {
			if (ec == asio::error::eof ||
			    ec == asio::ssl::error::stream_truncated) {
				self->finish_client();
				return;
			}

			if (ec) {
				std::cerr << "[CHAT] Client read: "
					  << ec.message() << '\n';
				self->stop();
				return;
			}

			try {
				self->pending_client_write_ =
					self->xmpp_parser_.feed(
						std::string_view(
							self->client_buffer_
								.data(),
							bytes));
			} catch (const std::exception &e) {
				std::cerr << "[XMPP] Parser error: " << e.what()
					  << '\n';
				self->stop();
				return;
			}

			if (self->pending_client_write_.empty()) {
				self->read_client();
				return;
			}

			asio::async_write(
				self->upstream_,
				asio::buffer(self->pending_client_write_),
				[self](error_code ec, std::size_t) {
					if (ec) {
						std::cerr << "[CHAT] Upstream "
							     "write: "
							  << ec.message()
							  << '\n';
						self->stop();
						return;
					}

					self->pending_client_write_.clear();
					self->read_client();
				});
		});
}

void proxy_session::read_upstream()
{
	auto self = shared_from_this();

	upstream_.async_read_some(
		asio::buffer(upstream_buffer_),
		[self](error_code ec, std::size_t bytes) {
			if (ec == asio::error::eof ||
			    ec == asio::ssl::error::stream_truncated) {
				self->finish_upstream();
				return;
			}

			if (ec) {
				std::cerr << "[CHAT] Upstream read: "
					  << ec.message() << '\n';
				self->stop();
				return;
			}

			asio::async_write(
				self->client_,
				asio::buffer(self->upstream_buffer_.data(),
					     bytes),
				[self](error_code ec, std::size_t) {
					if (ec) {
						std::cerr << "[CHAT] Client "
							     "write: "
							  << ec.message()
							  << '\n';
						self->stop();
						return;
					}

					self->read_upstream();
				});
		});
}

void proxy_session::finish_client()
{
	if (client_finished_ || stopped_) {
		return;
	}

	client_finished_ = true;

	error_code ec;

	upstream_.next_layer().shutdown(tcp::socket::shutdown_send, ec);

	if (upstream_finished_) {
		stop();
	}
}

void proxy_session::finish_upstream()
{
	if (upstream_finished_ || stopped_) {
		return;
	}

	upstream_finished_ = true;

	error_code ec;

	client_.next_layer().shutdown(tcp::socket::shutdown_send, ec);

	if (client_finished_) {
		stop();
	}
}

void proxy_session::stop()
{
	if (stopped_) {
		return;
	}

	stopped_ = true;

	error_code ec;

	resolver_.cancel();

	client_.next_layer().cancel(ec);
	client_.next_layer().close(ec);

	upstream_.next_layer().cancel(ec);
	upstream_.next_layer().close(ec);
}

} // namespace veil::network