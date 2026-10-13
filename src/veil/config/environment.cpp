#include <veil/config/environment.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace veil::config
{

namespace
{

std::string trim(std::string value)
{
	auto not_space = [](unsigned char ch) { return !std::isspace(ch); };

	value.erase(value.begin(),
		    std::find_if(value.begin(), value.end(), not_space));

	value.erase(
		std::find_if(value.rbegin(), value.rend(), not_space).base(),
		value.end());

	return value;
}

unsigned short parse_port(const std::string &value)
{
	std::size_t consumed = 0;
	unsigned long port = std::stoul(value, &consumed);

	if (consumed != value.size() || port == 0 || port > 65535) {
		throw std::runtime_error("Invalid port: " + value);
	}

	return static_cast<unsigned short>(port);
}

} // namespace

environment environment::load(const std::string &path)
{
	std::ifstream file(path);

	if (!file) {
		throw std::runtime_error("Unable to open environment file: " +
					 path);
	}

	std::unordered_map<std::string, std::string> values;
	std::string line;

	while (std::getline(file, line)) {
		line = trim(line);

		if (line.empty() || line.starts_with('#')) {
			continue;
		}

		const auto separator = line.find('=');

		if (separator == std::string::npos) {
			continue;
		}

		auto key = trim(line.substr(0, separator));
		auto value = trim(line.substr(separator + 1));

		if (value.size() >= 2 &&
		    ((value.front() == '"' && value.back() == '"') ||
		     (value.front() == '\'' && value.back() == '\''))) {
			value = value.substr(1, value.size() - 2);
		}

		values[std::move(key)] = std::move(value);
	}

	auto require = [&](const std::string &key) {
		auto it = values.find(key);

		if (it == values.end() || it->second.empty()) {
			throw std::runtime_error(
				"Missing environment variable: " + key);
		}

		return it->second;
	};

	environment env;

	env.host = require("VEIL_HOST");
	env.cert_file = require("VEIL_CERT_FILE");
	env.key_file = require("VEIL_KEY_FILE");
	env.ca_file = require("VEIL_CA_FILE");

	if (auto it = values.find("VEIL_CHAT_PORT"); it != values.end()) {
		env.chat_port = parse_port(it->second);
	}

	if (auto it = values.find("VEIL_CONFIG_PORT"); it != values.end()) {
		env.config_port = parse_port(it->second);
	}

	return env;
}

} // namespace veil::config