#ifndef VEIL_RIOT_LAUNCHER_HPP
#define VEIL_RIOT_LAUNCHER_HPP

#include <filesystem>
#include <string>

namespace veil::riot {

class RiotLauncher {
public:
    explicit RiotLauncher(std::filesystem::path executable);

    void launch_league(const std::string& config_url = "") const;

private:
    std::filesystem::path executable_;
};

}

#endif // VEIL_RIOT_LAUNCHER_HPP