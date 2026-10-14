#include <veil/xmpp/presence_filter.hpp>

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace veil::xmpp
{

namespace
{

void set_text(pugi::xml_node parent, const char *child_name, const char *value)
{
    auto child = parent.child(child_name);

    if (!child)
    {
        child = parent.append_child(child_name);
    }

    child.text().set(value);
}

void remove_game(pugi::xml_node games, const char *name)
{
    if (games)
    {
        games.remove_child(name);
    }
}

} // namespace

presence_filter::presence_filter(bool invisible, bool allow_directed_presence)
    : invisible_(invisible), allow_directed_presence_(allow_directed_presence)
{
}

std::string presence_filter::process(std::string_view stanza) const
{
    auto logger = spdlog::get("chat");

    if (!invisible_)
    {
        return std::string(stanza);
    }

    pugi::xml_document document;

    const auto result =
        document.load_buffer(stanza.data(), stanza.size(),
                             pugi::parse_default | pugi::parse_fragment);

    if (!result)
    {
        throw std::runtime_error("Failed to parse outgoing XMPP stanza");
    }

    auto presence = document.child("presence");

    if (!presence)
    {
        return std::string(stanza);
    }

    if (presence.attribute("to"))
    {
        if (allow_directed_presence_)
        {
            return std::string(stanza);
        }

        logger->debug("Directed presence suppressed");

        return {};
    }

    const std::string_view type = presence.attribute("type").as_string();

    if (type == "unavailable" || type == "error")
    {
        return std::string(stanza);
    }

    set_text(presence, "show", "offline");

    auto games = presence.child("games");
    auto league = games.child("league_of_legends");

    if (league)
    {
        auto status = league.child("st");

        if (status)
        {
            status.text().set("offline");
        }
    }

    presence.remove_child("status");

    remove_game(games, "league_of_legends");
    remove_game(games, "bacon");
    remove_game(games, "lion");
    remove_game(games, "keystone");
    remove_game(games, "riot_client");
    remove_game(games, "teamfighttactics");
    remove_game(games, "valorant");

    std::ostringstream output;

    document.save(output, "", pugi::format_raw | pugi::format_no_declaration);

    logger->debug("Outgoing presence rewritten to offline");

    return output.str();
}

} // namespace veil::xmpp
