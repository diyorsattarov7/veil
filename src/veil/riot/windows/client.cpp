#include <veil/riot/client.hpp>

#include <boost/json.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace veil::riot
{

namespace
{

constexpr const char *executable_name = "RiotClientServices.exe";

std::optional<std::filesystem::path> validate(const std::filesystem::path &path)
{
    std::error_code ec;

    if (std::filesystem::is_regular_file(path, ec))
    {
        return path;
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> from_metadata()
{
    const char *program_data = std::getenv("ProgramData");

    if (!program_data)
    {
        return std::nullopt;
    }

    const auto metadata_path = std::filesystem::path(program_data) /
                               "Riot Games" / "RiotClientInstalls.json";

    std::ifstream file(metadata_path);

    if (!file)
    {
        return std::nullopt;
    }

    try
    {
        std::string contents(std::istreambuf_iterator<char>(file),
                             std::istreambuf_iterator<char>{});

        auto json = boost::json::parse(contents);

        if (!json.is_object())
        {
            return std::nullopt;
        }

        const auto &object = json.as_object();
        const auto *value = object.if_contains("rc_live");

        if (!value || !value->is_string())
        {
            return std::nullopt;
        }

        const std::string path(value->as_string().c_str());

        return validate(path);
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::optional<std::filesystem::path> from_default_paths()
{
    std::vector<std::filesystem::path> candidates{
        R"(C:\Riot Games\Riot Client\RiotClientServices.exe)"};

    if (const char *program_files = std::getenv("ProgramFiles"))
    {
        candidates.emplace_back(std::filesystem::path(program_files) /
                                "Riot Games" / "Riot Client" / executable_name);
    }

    if (const char *program_files_x86 = std::getenv("ProgramFiles(x86)"))
    {
        candidates.emplace_back(std::filesystem::path(program_files_x86) /
                                "Riot Games" / "Riot Client" / executable_name);
    }

    for (const auto &candidate : candidates)
    {
        if (auto path = validate(candidate))
        {
            return path;
        }
    }

    return std::nullopt;
}

} // namespace

std::filesystem::path riot_client::discover()
{
    auto logger = spdlog::get("riot");

    if (auto path = from_metadata())
    {
        logger->debug("Riot Client discovered through installation metadata");
        return *path;
    }

    if (auto path = from_default_paths())
    {
        logger->debug("Riot Client discovered through default paths");
        return *path;
    }

    throw std::runtime_error("Unable to locate RiotClientServices.exe");
}

} // namespace veil::riot
