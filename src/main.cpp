#include <veil/config/environment.hpp>
#include <veil/network/http_server.hpp>
#include <veil/network/tcp_proxy.hpp>
#include <veil/riot/client.hpp>
#include <veil/riot/launcher.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

int main()
{
    auto logger = spdlog::stdout_color_mt("veil");
    auto chat_logger = spdlog::stdout_color_mt("chat");
    auto riot_logger = spdlog::stdout_color_mt("riot");
    auto http_logger = spdlog::stdout_color_mt("http");

    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");

    spdlog::set_level(spdlog::level::debug);

    try
    {
        namespace asio = boost::asio;

        logger->info("Starting Veil");

        const auto env = veil::config::environment::load();

        logger->debug("Environment loaded");

        asio::io_context io;

        asio::ssl::context chat_context(asio::ssl::context::tls_server);

        chat_context.use_certificate_chain_file(env.cert_file);

        chat_context.use_private_key_file(env.key_file,
                                          asio::ssl::context::pem);

        logger->debug("TLS certificate and private key loaded");

        std::unique_ptr<veil::network::tcp_proxy> chat_proxy;

        std::string chat_host;
        unsigned short chat_port = 0;

        veil::network::http_server server(io, env.config_port, env.host,
                                          env.chat_port, env.ca_file);

        server.set_chat_server_handler(
            [&](std::string host, unsigned short port) -> bool
            {
                if (chat_proxy)
                {
                    if (host != chat_host || port != chat_port)
                    {
                        chat_logger->warn(
                            "Upstream changed from {}:{} to {}:{}", chat_host,
                            chat_port, host, port);

                        return false;
                    }

                    return true;
                }

                try
                {
                    auto proxy = std::make_unique<veil::network::tcp_proxy>(
                        io, env.chat_port, chat_context, host, port,
                        env.ca_file);

                    proxy->start();

                    chat_host = std::move(host);
                    chat_port = port;
                    chat_proxy = std::move(proxy);

                    chat_logger->info("Proxy listening on port {}",
                                      env.chat_port);

                    chat_logger->debug("Upstream: {}:{}", chat_host, chat_port);

                    chat_logger->debug("Redirect: {}:{} -> {}:{}", chat_host,
                                       chat_port, env.host, env.chat_port);

                    return true;
                }
                catch (const std::exception &e)
                {
                    chat_logger->error("Failed to start proxy: {}", e.what());

                    return false;
                }
            });

        server.start();

        logger->debug("Configuration server started on port {}",
                      env.config_port);

        auto executable = veil::riot::riot_client::discover();

        riot_logger->debug("Riot Client executable: {}", executable.string());

        veil::riot::riot_launcher launcher(executable);

        const std::string config_url =
            "http://127.0.0.1:" + std::to_string(env.config_port);

        launcher.launch_league(config_url);

        riot_logger->info("Riot Client launch requested");

        io.run();

        logger->info("Veil stopped");
    }
    catch (const std::exception &e)
    {
        logger->critical("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
