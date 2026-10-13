#ifndef VEIL_RIOT_CLIENT_HPP
#define VEIL_RIOT_CLIENT_HPP

#include <filesystem>

namespace veil::riot {

class RiotClient {
public:
    [[nodiscard]] static std::filesystem::path discover();
};

}

#endif // VEIL_RIOT_CLIENT_HPP