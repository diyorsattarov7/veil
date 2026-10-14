#include <veil/network/http_server.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <boost/system/error_code.hpp>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace veil::network
{

namespace
{

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using tcp = asio::ip::tcp;
using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;

constexpr auto upstream_host = "clientconfig.rpg.riotgames.com";

std::shared_ptr<spdlog::logger> http_logger()
{
    auto logger = spdlog::get("http");
    return logger ? logger : spdlog::default_logger();
}

std::shared_ptr<spdlog::logger> chat_logger()
{
    auto logger = spdlog::get("chat");
    return logger ? logger : spdlog::default_logger();
}

bool is_config_request(const std::string &target)
{
    return target.starts_with("/api/v1/config/public?") ||
           target.starts_with("/api/v1/config/player?");
}

bool patch_chat_config(std::string &body,
                       const http_server::ChatServerHandler &on_chat_server,
                       const std::string &local_chat_host,
                       unsigned short local_chat_port)
{
    boost::system::error_code ec;
    auto document = json::parse(body, ec);

    if (ec || !document.is_object())
    {
        chat_logger()->debug(
            "Chat patch skipped: invalid JSON or non-object response");
        return false;
    }

    auto &config = document.as_object();
    auto *host = config.if_contains("chat.host");
    auto *port = config.if_contains("chat.port");

    if (!host || !host->is_string() || !port || !port->is_int64())
    {
        chat_logger()->trace(
            "Chat patch skipped: response has no chat endpoint fields");
        return false;
    }

    const std::string original_host(host->as_string());
    const auto original_port = port->as_int64();

    if (original_host.empty() || original_port < 1 || original_port > 65535)
    {
        chat_logger()->warn("Chat patch skipped: invalid upstream endpoint");
        return false;
    }

    chat_logger()->debug("Chat upstream endpoint: {}:{}", original_host,
                         original_port);

    auto *affinities = config.if_contains("chat.affinities");
    if (!affinities || !affinities->is_object())
    {
        chat_logger()->warn("Chat patch skipped: missing affinity map");
        return false;
    }

    auto &affinity_map = affinities->as_object();
    bool matched = false;

    for (const auto &entry : affinity_map)
    {
        if (!entry.value().is_string())
        {
            chat_logger()->debug(
                "Chat affinity '{}' has a non-string value",
                std::string_view(entry.key().data(), entry.key().size()));
            continue;
        }

        const auto &value = entry.value().as_string();
        const std::string_view endpoint(value.data(), value.size());
        const bool is_match = endpoint == std::string_view(original_host);

        chat_logger()->trace(
            "Chat affinity '{}' -> {} (match={})",
            std::string_view(entry.key().data(), entry.key().size()), endpoint,
            is_match);
        matched |= is_match;
    }

    if (!matched)
    {
        chat_logger()->warn(
            "Chat patch skipped: no affinity matches upstream host");
        return false;
    }

    if (!on_chat_server ||
        !on_chat_server(original_host,
                        static_cast<unsigned short>(original_port)))
    {
        chat_logger()->warn("Chat patch skipped: proxy unavailable");
        return false;
    }

    *host = local_chat_host;
    *port = static_cast<std::int64_t>(local_chat_port);

    std::size_t changed = 0;
    for (auto &entry : affinity_map)
    {
        if (!entry.value().is_string())
        {
            continue;
        }

        const auto &value = entry.value().as_string();
        if (std::string_view(value.data(), value.size()) ==
            std::string_view(original_host))
        {
            entry.value() = local_chat_host;
            ++changed;
        }
    }

    body = json::serialize(document);
    chat_logger()->debug(
        "Chat endpoint rewritten: {}:{} -> {}:{} ({} affinities)",
        original_host, original_port, local_chat_host, local_chat_port,
        changed);
    return true;
}

Response fetch_config(const std::string &target, const std::string &user_agent,
                      const std::string &authorization,
                      const std::string &entitlements,
                      const std::string &ca_file)
{
    asio::io_context io;

    asio::ssl::context ssl_context(asio::ssl::context::tls_client);

    ssl_context.set_verify_mode(asio::ssl::verify_peer);
    ssl_context.load_verify_file(ca_file);

    tcp::resolver resolver(io);

    beast::ssl_stream<beast::tcp_stream> stream(io, ssl_context);

    if (!SSL_set_tlsext_host_name(stream.native_handle(), upstream_host))
    {
        throw beast::system_error(static_cast<int>(ERR_get_error()),
                                  asio::error::get_ssl_category());
    }

    stream.set_verify_callback(
        asio::ssl::host_name_verification(upstream_host));

    auto endpoints = resolver.resolve(upstream_host, "443");

    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(15));

    beast::get_lowest_layer(stream).connect(endpoints);

    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(15));

    stream.handshake(asio::ssl::stream_base::client);

    http::request<http::empty_body> request{http::verb::get, target, 11};

    request.set(http::field::host, upstream_host);

    request.set(http::field::user_agent,
                user_agent.empty() ? "Veil/0.1" : user_agent);

    request.set(http::field::accept, "application/json");
    request.set(http::field::connection, "close");

    if (!authorization.empty())
    {
        request.set(http::field::authorization, authorization);
    }

    if (!entitlements.empty())
    {
        request.set("X-Riot-Entitlements-JWT", entitlements);
    }

    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(15));

    http::write(stream, request);

    beast::flat_buffer buffer;
    Response response;

    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(15));

    http::read(stream, buffer, response);

    beast::error_code ec;
    stream.shutdown(ec);

    if (ec && ec != asio::error::eof &&
        ec != asio::ssl::error::stream_truncated)
    {
        http_logger()->debug("Upstream TLS shutdown: {}", ec.message());
    }

    return response;
}

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
public:
    HttpSession(tcp::socket socket,
                http_server::ChatServerHandler chat_server_handler,
                std::string local_chat_host, unsigned short local_chat_port,
                std::string ca_file)
        : stream_(std::move(socket)),
          chat_server_handler_(std::move(chat_server_handler)),
          local_chat_host_(std::move(local_chat_host)),
          local_chat_port_(local_chat_port), ca_file_(std::move(ca_file))
    {
    }

    void start()
    {
        stream_.expires_after(std::chrono::seconds(30));

        http::async_read(
            stream_, buffer_, request_,
            [self = shared_from_this()](beast::error_code ec, std::size_t)
            {
                if (ec)
                {
                    if (ec != http::error::end_of_stream)
                    {
                        http_logger()->warn("HTTP read failed: {}",
                                            ec.message());
                    }
                    return;
                }

                self->respond();
            });
    }

private:
    void respond()
    {
        const std::string target(request_.target());
        const std::string path = target.substr(0, target.find('?'));
        http_logger()->debug("{} {} (query={}, authorization={})",
                             std::string_view(request_.method_string().data(),
                                              request_.method_string().size()),
                             path, target.find('?') != std::string::npos,
                             request_.find(http::field::authorization) !=
                                 request_.end());

        if (request_.method() == http::verb::get && target == "/health")
        {
            response_.result(http::status::ok);
            response_.body() = R"({"status":"ok","application":"veil"})";

            send();
            return;
        }

        if (request_.method() != http::verb::get || !is_config_request(target))
        {
            response_.result(http::status::not_found);
            response_.body() = R"({"error":"not found"})";
            http_logger()->debug("Rejected route: {}", path);

            send();
            return;
        }

        const std::string user_agent = header(http::field::user_agent);

        const std::string authorization = header(http::field::authorization);

        const std::string entitlements = header("X-Riot-Entitlements-JWT");

        auto self = shared_from_this();

        std::thread(
            [self, target, user_agent, authorization, entitlements]()
            {
                try
                {
                    auto upstream =
                        fetch_config(target, user_agent, authorization,
                                     entitlements, self->ca_file_);

                    http_logger()->debug("Upstream config response: {} {}",
                                         upstream.result_int(),
                                         target.substr(0, target.find('?')));

                    asio::post(
                        self->stream_.get_executor(),
                        [self, upstream = std::move(upstream)]() mutable
                        {
                            if (upstream.result() == http::status::ok)
                            {
                                try
                                {
                                    const bool patched = patch_chat_config(
                                        upstream.body(),
                                        self->chat_server_handler_,
                                        self->local_chat_host_,
                                        self->local_chat_port_);
                                    if (patched)
                                        chat_logger()->debug(
                                            "Chat config patch applied");
                                }
                                catch (const std::exception &e)
                                {
                                    chat_logger()->error(
                                        "Chat configuration rewrite failed: {}",
                                        e.what());
                                }
                            }

                            self->response_.result(upstream.result());
                            self->response_.body() = std::move(upstream.body());

                            if (upstream.find(http::field::content_type) !=
                                upstream.end())
                            {
                                self->response_.set(
                                    http::field::content_type,
                                    upstream[http::field::content_type]);
                            }

                            self->send();
                        });
                }
                catch (const std::exception &e)
                {
                    http_logger()->error(
                        "Upstream configuration request failed: {}", e.what());

                    asio::post(self->stream_.get_executor(),
                               [self]()
                               {
                                   self->response_.result(
                                       http::status::bad_gateway);

                                   self->response_.body() =
                                       R"({"error":"upstream request failed"})";

                                   self->send();
                               });
                }
            })
            .detach();
    }

    std::string header(http::field field) const
    {
        auto it = request_.find(field);

        if (it == request_.end())
        {
            return {};
        }

        return std::string(it->value());
    }

    std::string header(beast::string_view name) const
    {
        auto it = request_.find(name);

        if (it == request_.end())
        {
            return {};
        }

        return std::string(it->value());
    }

    void send()
    {
        response_.version(request_.version());
        response_.keep_alive(false);
        response_.set(http::field::server, "Veil/0.1");

        if (response_.find(http::field::content_type) == response_.end())
        {
            response_.set(http::field::content_type, "application/json");
        }

        response_.prepare_payload();
        http_logger()->debug("HTTP response: {} ({} bytes)",
                             response_.result_int(), response_.body().size());

        stream_.expires_after(std::chrono::seconds(30));

        http::async_write(
            stream_, response_,
            [self = shared_from_this()](beast::error_code ec, std::size_t)
            {
                if (ec)
                {
                    http_logger()->warn("HTTP write failed: {}", ec.message());
                }

                beast::error_code ignored;

                self->stream_.socket().shutdown(tcp::socket::shutdown_send,
                                                ignored);
            });
    }

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;

    Request request_;
    Response response_;

    http_server::ChatServerHandler chat_server_handler_;
    std::string local_chat_host_;
    unsigned short local_chat_port_;
    std::string ca_file_;
};

} // namespace

http_server::http_server(asio::io_context &io, unsigned short port,
                         std::string chat_host, unsigned short chat_port,
                         std::string ca_file)
    : acceptor_(io, tcp::endpoint(tcp::v4(), port)),
      local_chat_host_(std::move(chat_host)), local_chat_port_(chat_port),
      ca_file_(std::move(ca_file))
{
}

void http_server::set_chat_server_handler(ChatServerHandler handler)
{
    chat_server_handler_ = std::move(handler);
}

void http_server::start() { accept(); }

void http_server::accept()
{
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                std::make_shared<HttpSession>(
                    std::move(socket), chat_server_handler_, local_chat_host_,
                    local_chat_port_, ca_file_)
                    ->start();
            }
            else if (ec != asio::error::operation_aborted)
            {
                http_logger()->error("HTTP accept failed: {}", ec.message());
            }

            if (acceptor_.is_open())
            {
                accept();
            }
        });
}

} // namespace veil::network
