#include <veil/riot/launcher.hpp>

#include <Windows.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace veil::riot {

RiotLauncher::RiotLauncher(std::filesystem::path executable)
    : executable_(std::move(executable)) {}

void RiotLauncher::launch_league(const std::string& config_url) const {
    if (!std::filesystem::is_regular_file(executable_)) {
        throw std::runtime_error("Riot Client executable not found");
    }

    std::wstring arguments =
        L"\"" + executable_.wstring() + L"\" "
        L"--launch-product=league_of_legends "
        L"--launch-patchline=live "
        L"--allow-direct-launch";

    if (!config_url.empty()) {
        std::wstring url(config_url.begin(), config_url.end());

        arguments += L" --client-config-url=\"" + url + L"\"";
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process{};

    if (!CreateProcessW(
            executable_.c_str(),
            arguments.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &startup,
            &process
        )) {
        throw std::runtime_error("Failed to launch Riot Client");
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}

}