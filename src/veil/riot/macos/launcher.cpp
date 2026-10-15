#include <veil/riot/launcher.hpp>

#include <spdlog/spdlog.h>

#include <spawn.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern char **environ;

namespace veil::riot
{

riot_launcher::riot_launcher(std::filesystem::path executable)
    : executable_(std::move(executable))
{
}

void riot_launcher::launch_league(const std::string &config_url) const
{
	auto logger = spdlog::get("riot");

	const auto bundle =
		executable_.parent_path().parent_path().parent_path();

	std::vector<std::string> arguments = {
		"/usr/bin/open",
		"-n",
		bundle.string(),
		"--args",
		"--launch-product=league_of_legends",
		"--launch-patchline=live",
		"--allow-direct-launch"};

	if (!config_url.empty()) {
		arguments.push_back("--client-config-url=" + config_url);
	}

	std::vector<char *> argv;

	for (auto &argument : arguments) {
		argv.push_back(argument.data());
	}

	argv.push_back(nullptr);

	pid_t pid = 0;

	logger->debug(
		"Requesting Riot Client launch through macOS Launch Services");

	const int result = posix_spawn(
		&pid, "/usr/bin/open", nullptr, nullptr, argv.data(), environ);

	if (result != 0) {
		throw std::runtime_error("Failed to launch Riot Client");
	}

	logger->debug("Launch Services process started (PID {})", pid);
}

} // namespace veil::riot