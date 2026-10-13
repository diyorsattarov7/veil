#ifndef VEIL_CONFIG_ENVIRONMENT_HPP
#define VEIL_CONFIG_ENVIRONMENT_HPP

#include <string>

namespace veil::config
{

struct environment {
	std::string host;

	unsigned short chat_port = 5000;
	unsigned short config_port = 5001;

	std::string cert_file;
	std::string key_file;
	std::string ca_file;

	static environment load(const std::string &path = ".env");
};

} // namespace veil::config

#endif