#ifndef VEIL_RIOT_LAUNCHER_HPP
#define VEIL_RIOT_LAUNCHER_HPP

#include <filesystem>
#include <string>

namespace veil::riot
{

class riot_launcher
{
public:
	explicit riot_launcher(std::filesystem::path executable);

	void launch_league(const std::string &config_url = "") const;

private:
	std::filesystem::path executable_;
};

} // namespace veil::riot

#endif // VEIL_RIOT_LAUNCHER_HPP