
#include <veil/xmpp/presence_filter.hpp>

#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

class PresenceFilterTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		if (!spdlog::get("chat")) {
			auto sink =
				std::make_shared<spdlog::sinks::null_sink_mt>();
			auto logger =
				std::make_shared<spdlog::logger>("chat", sink);

			spdlog::register_logger(logger);
		}
	}

	static pugi::xml_document parse(const std::string &xml)
	{
		pugi::xml_document document;

		const auto result = document.load_string(xml.c_str());

		if (!result) {
			throw std::runtime_error("Failed to parse test XML");
		}

		return document;
	}
};

TEST_F(PresenceFilterTest, RewritesAvailablePresenceToOffline)
{
	veil::xmpp::presence_filter filter;

	const auto output = filter.process("<presence/>");
	const auto document = parse(output);

	EXPECT_STREQ(
		document.child("presence").child("show").text().as_string(),
		"offline");
}

TEST_F(PresenceFilterTest, ReplacesExistingShow)
{
	veil::xmpp::presence_filter filter;

	const auto output =
		filter.process("<presence><show>chat</show></presence>");

	const auto document = parse(output);
	const auto presence = document.child("presence");

	EXPECT_STREQ(presence.child("show").text().as_string(), "offline");

	EXPECT_EQ(std::distance(presence.children("show").begin(),
				presence.children("show").end()),
		  1);
}

TEST_F(PresenceFilterTest, RemovesStatus)
{
	veil::xmpp::presence_filter filter;

	const auto output = filter.process(
		"<presence><status>Playing League</status></presence>");

	const auto document = parse(output);

	EXPECT_FALSE(document.child("presence").child("status"));
}

TEST_F(PresenceFilterTest, RemovesGamePresence)
{
	veil::xmpp::presence_filter filter;

	const auto output = filter.process(
		"<presence>"
		"  <games>"
		"    <league_of_legends><st>inGame</st></league_of_legends>"
		"    <valorant/>"
		"    <teamfighttactics/>"
		"    <riot_client/>"
		"  </games>"
		"</presence>");

	const auto document = parse(output);
	const auto games = document.child("presence").child("games");

	EXPECT_FALSE(games.child("league_of_legends"));
	EXPECT_FALSE(games.child("valorant"));
	EXPECT_FALSE(games.child("teamfighttactics"));
	EXPECT_FALSE(games.child("riot_client"));
}

TEST_F(PresenceFilterTest, RemovesAllKnownGameNodes)
{
	veil::xmpp::presence_filter filter;

	const auto output = filter.process("<presence>"
					   "  <games>"
					   "    <league_of_legends/>"
					   "    <bacon/>"
					   "    <lion/>"
					   "    <keystone/>"
					   "    <riot_client/>"
					   "    <teamfighttactics/>"
					   "    <valorant/>"
					   "  </games>"
					   "</presence>");

	const auto document = parse(output);
	const auto games = document.child("presence").child("games");

	EXPECT_TRUE(games);
	EXPECT_FALSE(games.first_child());
}

TEST_F(PresenceFilterTest, PreservesUnavailablePresence)
{
	veil::xmpp::presence_filter filter;

	constexpr std::string_view input = "<presence type=\"unavailable\"/>";

	EXPECT_EQ(filter.process(input), input);
}

TEST_F(PresenceFilterTest, PreservesErrorPresence)
{
	veil::xmpp::presence_filter filter;

	constexpr std::string_view input = "<presence type=\"error\"/>";

	EXPECT_EQ(filter.process(input), input);
}

TEST_F(PresenceFilterTest, PreservesDirectedPresenceByDefault)
{
	veil::xmpp::presence_filter filter;

	constexpr std::string_view input =
		"<presence "
		"to=\"friend@example.com\"><show>chat</show></presence>";

	EXPECT_EQ(filter.process(input), input);
}

TEST_F(PresenceFilterTest, SuppressesDirectedPresenceWhenDisabled)
{
	veil::xmpp::presence_filter filter(true, false);

	constexpr std::string_view input =
		"<presence to=\"friend@example.com\"/>";

	EXPECT_TRUE(filter.process(input).empty());
}

TEST_F(PresenceFilterTest, PreservesOriginalWhenInvisibleModeDisabled)
{
	veil::xmpp::presence_filter filter(false);

	constexpr std::string_view input =
		"<presence><show>chat</show><status>Online</status></presence>";

	EXPECT_EQ(filter.process(input), input);
}

TEST_F(PresenceFilterTest, RejectsMalformedXML)
{
	veil::xmpp::presence_filter filter;

	EXPECT_THROW(filter.process("<presence><show></presence>"),
		     std::runtime_error);
}

TEST_F(PresenceFilterTest, PreservesNonPresenceStanzas)
{
	veil::xmpp::presence_filter filter;

	constexpr std::string_view input =
		"<message "
		"to=\"friend@example.com\"><body>Hello</body></message>";

	EXPECT_EQ(filter.process(input), input);
}

} // namespace