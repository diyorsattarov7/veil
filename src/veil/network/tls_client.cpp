#include <veil/network/tls_client.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <spdlog/spdlog.h>

#include <openssl/ssl.h>

#include <stdexcept>

namespace veil::network
{

tls_client::tls_client(boost::asio::io_context &io)
    : ssl_context_(boost::asio::ssl::context::tls_client), resolver_(io),
      stream_(io, ssl_context_)
{
    ssl_context_.load_verify_file("certs/cacert.pem");
    stream_.set_verify_mode(boost::asio::ssl::verify_peer);
}

void tls_client::connect(const std::string &host, unsigned short port)
{
    auto logger = spdlog::get("http");

    logger->debug("Connecting to {}:{}", host, port);

    if (!SSL_set_tlsext_host_name(stream_.native_handle(), host.c_str()))
    {
        throw std::runtime_error("Failed to set TLS SNI hostname");
    }

    stream_.set_verify_callback(boost::asio::ssl::host_name_verification(host));

    auto endpoints = resolver_.resolve(host, std::to_string(port));

    boost::asio::connect(stream_.lowest_layer(), endpoints);

    logger->debug("TCP connection established: {}:{}", host, port);

    stream_.handshake(boost::asio::ssl::stream_base::client);

    logger->debug("TLS handshake established: {}:{}", host, port);
}

std::string tls_client::get(const std::string &host, const std::string &target)
{
    namespace http = boost::beast::http;

    auto logger = spdlog::get("http");

    // Avoid logging the target because it may contain query parameters.
    logger->debug("Sending HTTP GET to {}", host);

    http::request<http::empty_body> request{http::verb::get, target, 11};

    request.set(http::field::host, host);
    request.set(http::field::user_agent, "Veil/0.1");

    http::write(stream_, request);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> response;

    http::read(stream_, buffer, response);

    logger->debug("HTTP response status: {}", response.result_int());

    if (response.result() != http::status::ok)
    {
        throw std::runtime_error("HTTP request failed: " +
                                 std::to_string(response.result_int()));
    }

    return response.body();
}

} // namespace veil::network
