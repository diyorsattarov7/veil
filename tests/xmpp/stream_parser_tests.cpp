
#include <veil/xmpp/stream_parser.hpp>

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

using veil::xmpp::stream_parser;

class StreamParserTest : public ::testing::Test
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

	static void expect_offline(const std::string &xml)
	{
		const auto document = parse(xml);
		const auto presence = document.child("presence");

		ASSERT_TRUE(presence);
		EXPECT_STREQ(presence.child("show").text().as_string(),
			     "offline");
	}
};

TEST_F(StreamParserTest, RewritesCompletePresence)
{
	stream_parser parser;

	const auto output = parser.feed("<presence/>");

	expect_offline(output);
}

TEST_F(StreamParserTest, PreservesMessages)
{
	stream_parser parser;

	constexpr std::string_view input =
		"<message><body>Hello</body></message>";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, PreservesIqStanzas)
{
	stream_parser parser;

	constexpr std::string_view input = "<iq type=\"get\" id=\"1\"/>";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, BuffersIncompletePresence)
{
	stream_parser parser;

	EXPECT_TRUE(parser.feed("<presence><show>").empty());

	const auto output = parser.feed("chat</show></presence>");

	expect_offline(output);
}

TEST_F(StreamParserTest, HandlesFragmentedOpeningTag)
{
	stream_parser parser;

	EXPECT_TRUE(parser.feed("<pres").empty());
	EXPECT_TRUE(parser.feed("ence><show>chat").empty());

	const auto output = parser.feed("</show></presence>");

	expect_offline(output);
}

TEST_F(StreamParserTest, HandlesMultipleStanzas)
{
	stream_parser parser;

	const auto output = parser.feed("<message id=\"1\"/>"
					"<presence/>"
					"<iq id=\"2\"/>");

	EXPECT_NE(output.find("<message id=\"1\"/>"), std::string::npos);
	EXPECT_NE(output.find("<iq id=\"2\"/>"), std::string::npos);
	EXPECT_NE(output.find("<show>offline</show>"), std::string::npos);
}

TEST_F(StreamParserTest, BuffersIncompleteSecondStanza)
{
	stream_parser parser;

	const auto first = parser.feed("<message id=\"1\"/><presence><show>");

	EXPECT_EQ(first, "<message id=\"1\"/>");

	const auto second = parser.feed("chat</show></presence>");

	expect_offline(second);
}

TEST_F(StreamParserTest, PreservesXmlDeclaration)
{
	stream_parser parser;

	constexpr std::string_view input = "<?xml version=\"1.0\"?>";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, BuffersIncompleteXmlDeclaration)
{
	stream_parser parser;

	EXPECT_TRUE(parser.feed("<?xml version=").empty());

	EXPECT_EQ(parser.feed("\"1.0\"?>"), "<?xml version=\"1.0\"?>");
}

TEST_F(StreamParserTest, PreservesStreamOpeningTag)
{
	stream_parser parser;

	constexpr std::string_view input =
		"<stream:stream "
		"xmlns:stream=\"http://etherx.jabber.org/streams\">";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, PreservesStreamClosingTag)
{
	stream_parser parser;

	constexpr std::string_view input = "</stream:stream>";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, BuffersIncompleteStreamOpeningTag)
{
	stream_parser parser;

	EXPECT_TRUE(parser.feed("<stream:stream xmlns=").empty());

	EXPECT_EQ(parser.feed("\"jabber:client\">"),
		  "<stream:stream xmlns=\"jabber:client\">");
}

TEST_F(StreamParserTest, PreservesComments)
{
	stream_parser parser;

	constexpr std::string_view input = "<!-- XMPP comment -->";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, HandlesCdataWithinMessage)
{
	stream_parser parser;

	constexpr std::string_view input =
		"<message><body><![CDATA[<presence/>]]></body></message>";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, HandlesGreaterThanInsideAttribute)
{
	stream_parser parser;

	constexpr std::string_view input = "<message value=\"a > b\"/>";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, PreservesPresenceInOnlineMode)
{
	stream_parser parser(stream_parser::Mode::Online);

	constexpr std::string_view input =
		"<presence><show>chat</show></presence>";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, SwitchesToInvisibleMode)
{
	stream_parser parser(stream_parser::Mode::Online);

	parser.set_mode(stream_parser::Mode::Invisible);

	expect_offline(parser.feed("<presence/>"));
}

TEST_F(StreamParserTest, SwitchesToOnlineMode)
{
	stream_parser parser;

	parser.set_mode(stream_parser::Mode::Online);

	constexpr std::string_view input =
		"<presence><show>chat</show></presence>";

	EXPECT_EQ(parser.feed(input), input);
}

TEST_F(StreamParserTest, ResetDiscardsBufferedData)
{
	stream_parser parser;

	EXPECT_TRUE(parser.feed("<presence><show>").empty());

	parser.reset();

	expect_offline(parser.feed("<presence/>"));
}

TEST_F(StreamParserTest, RejectsUnexpectedClosingTag)
{
	stream_parser parser;

	EXPECT_THROW(parser.feed("</presence>"), std::runtime_error);
}

TEST_F(StreamParserTest, RejectsOversizedInput)
{
	stream_parser parser;

	const std::string input(1024 * 1024 + 1, 'x');

	EXPECT_THROW(parser.feed(input), std::runtime_error);
}

TEST_F(StreamParserTest, RejectsAccumulatedBufferOverflow)
{
	stream_parser parser;

	// Leave a large incomplete XML stanza in the buffer.
	const std::string input =
		"<message><body>" + std::string(1024 * 1024 - 32, 'x');

	EXPECT_TRUE(parser.feed(input).empty());

	EXPECT_THROW(parser.feed(std::string(64, 'x')), std::runtime_error);
}

} // namespace