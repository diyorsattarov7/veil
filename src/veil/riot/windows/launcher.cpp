#include <veil/riot/launcher.hpp>

#include <spdlog/spdlog.h>

#include <Windows.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace veil::riot
{

riot_launcher::riot_launcher(std::filesystem::path executable)
    : executable_(std::move(executable))
{
}

void riot_launcher::launch_league(const std::string &config_url) const
{
    auto logger = spdlog::get("riot");

    if (!std::filesystem::is_regular_file(executable_))
    {
        throw std::runtime_error("Riot Client executable not found");
    }

    std::wstring arguments = L"\"" + executable_.wstring() +
                             L"\" "
                             L"--launch-product=league_of_legends "
                             L"--launch-patchline=live "
                             L"--allow-direct-launch";

    if (!config_url.empty())
    {
        std::wstring url(config_url.begin(), config_url.end());

        arguments += L" --client-config-url=\"" + url + L"\"";
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process{};

    logger->debug("Starting Riot Client process");

    if (!CreateProcessW(executable_.c_str(), arguments.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &startup, &process))
    {
        throw std::runtime_error("Failed to launch Riot Client");
    }

    logger->debug("Riot Client process created (PID {})", process.dwProcessId);

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}

} // namespace veil::riot
