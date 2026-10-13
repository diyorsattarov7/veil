#include <veil/config/environment.hpp>
#include <veil/network/http_server.hpp>
#include <veil/network/tcp_proxy.hpp>
#include <veil/riot/client.hpp>
#include <veil/riot/launcher.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

int main()
{
	try {
		namespace asio = boost::asio;

		const auto env = veil::config::environment::load();

		asio::io_context io;

		asio::ssl::context chat_context(asio::ssl::context::tls_server);

		chat_context.use_certificate_chain_file(env.cert_file);

		chat_context.use_private_key_file(env.key_file,
						  asio::ssl::context::pem);

		std::cout << "[TLS] Certificate loaded\n";

		std::unique_ptr<veil::network::tcp_proxy> chat_proxy;

		std::string chat_host;
		unsigned short chat_port = 0;

		veil::network::http_server server(io,
						  env.config_port,
						  env.host,
						  env.chat_port,
						  env.ca_file);

		server.set_chat_server_handler([&](std::string host,
						   unsigned short port)
						       -> bool {
			if (chat_proxy) {
				return host == chat_host && port == chat_port;
			}

			try {
				auto proxy = std::make_unique<
					veil::network::tcp_proxy>(io,
								  env.chat_port,
								  chat_context,
								  host,
								  port);

				proxy->start();

				chat_host = std::move(host);
				chat_port = port;
				chat_proxy = std::move(proxy);

				std::cout << "[CHAT] Proxy listening on "
					  << env.chat_port << '\n';

				std::cout << "[CHAT] Upstream: " << chat_host
					  << ':' << chat_port << '\n';

				std::cout << "[CHAT] Redirect: " << chat_host
					  << ':' << chat_port << " -> "
					  << env.host << ':' << env.chat_port
					  << '\n';

				return true;
			} catch (const std::exception &e) {
				std::cerr
					<< "[CHAT] Proxy startup: " << e.what()
					<< '\n';
				return false;
			}
		});

		server.start();

		auto executable = veil::riot::riot_client::discover();
		veil::riot::riot_launcher launcher(executable);

		const std::string config_url =
			"http://127.0.0.1:" + std::to_string(env.config_port);

		launcher.launch_league(config_url);

		io.run();
	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}

	return 0;
}