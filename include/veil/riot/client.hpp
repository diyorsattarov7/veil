#ifndef VEIL_RIOT_CLIENT_HPP
#define VEIL_RIOT_CLIENT_HPP

#include <filesystem>

namespace veil::riot
{

class riot_client
{
public:
	[[nodiscard]] static std::filesystem::path discover();
};

} // namespace veil::riot

#endif // VEIL_RIOT_CLIENT_HPP