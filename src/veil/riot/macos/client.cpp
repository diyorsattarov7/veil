#include "veil/riot/client.hpp"

#include <filesystem>
#include <stdexcept>

namespace veil::riot
{

std::filesystem::path riot_client::discover()
{
    const std::filesystem::path executable =
        "/Users/Shared/Riot Games/Riot Client.app/"
        "Contents/MacOS/RiotClientServices";

    if (!std::filesystem::is_regular_file(executable))
        throw std::runtime_error("Riot Client executable not found");

    return executable;
}

} // namespace veil::riot
