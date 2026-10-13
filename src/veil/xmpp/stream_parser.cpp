#include <veil/xmpp/stream_parser.hpp>

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace veil::xmpp
{

namespace
{

bool starts_with(std::string_view text, std::string_view prefix)
{
	return text.size() >= prefix.size() &&
	       text.compare(0, prefix.size(), prefix) == 0;
}

bool is_space(char ch)
{
	return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

std::size_t find_tag_end(std::string_view text, std::size_t start)
{
	char quote = '\0';

	for (std::size_t i = start; i < text.size(); ++i) {
		const char ch = text[i];

		if (quote != '\0') {
			if (ch == quote) {
				quote = '\0';
			}
		} else if (ch == '"' || ch == '\'') {
			quote = ch;
		} else if (ch == '>') {
			return i;
		}
	}

	return std::string_view::npos;
}

bool self_closing(std::string_view text, std::size_t start, std::size_t end)
{
	while (end > start && is_space(text[end - 1])) {
		--end;
	}

	return end > start && text[end - 1] == '/';
}

bool is_presence(std::string_view stanza)
{
	if (!starts_with(stanza, "<presence")) {
		return false;
	}

	if (stanza.size() == 9) {
		return true;
	}

	const char next = stanza[9];

	return is_space(next) || next == '>' || next == '/';
}

} // namespace

stream_parser::stream_parser(Mode mode)
    : mode_(mode)
    , presence_filter_(mode == Mode::Invisible)
{
}

void stream_parser::reset()
{
	buffer_.clear();
}

void stream_parser::set_mode(Mode mode)
{
	mode_ = mode;
	presence_filter_ = presence_filter(mode == Mode::Invisible);
}

std::string stream_parser::feed(std::string_view data)
{
	if (data.size() > max_buffer_size ||
	    buffer_.size() > max_buffer_size - data.size()) {
		throw std::runtime_error("XMPP buffer limit exceeded");
	}

	buffer_.append(data);

	std::string output;
	process(output);

	return output;
}

void stream_parser::process(std::string &output)
{
	while (!buffer_.empty()) {
		if (buffer_[0] != '<') {
			const auto next = buffer_.find('<');

			if (next == std::string::npos) {
				output += buffer_;
				buffer_.clear();
				return;
			}

			output.append(buffer_, 0, next);
			buffer_.erase(0, next);
			continue;
		}

		if (starts_with(buffer_, "<?xml")) {
			const auto end = buffer_.find("?>");

			if (end == std::string::npos) {
				return;
			}

			const auto length = end + 2;

			output.append(buffer_, 0, length);
			buffer_.erase(0, length);
			continue;
		}

		if (starts_with(buffer_, "<stream:stream")) {
			const auto end = find_tag_end(buffer_, 0);

			if (end == std::string::npos) {
				return;
			}

			const auto length = end + 1;

			output.append(buffer_, 0, length);
			buffer_.erase(0, length);
			continue;
		}

		if (starts_with(buffer_, "</stream:stream")) {
			const auto end = find_tag_end(buffer_, 0);

			if (end == std::string::npos) {
				return;
			}

			const auto length = end + 1;

			output.append(buffer_, 0, length);
			buffer_.erase(0, length);
			continue;
		}

		if (starts_with(buffer_, "<!--")) {
			const auto end = buffer_.find("-->");

			if (end == std::string::npos) {
				return;
			}

			const auto length = end + 3;

			output.append(buffer_, 0, length);
			buffer_.erase(0, length);
			continue;
		}

		std::size_t pos = 0;
		std::size_t depth = 0;
		std::size_t stanza_end = std::string::npos;

		while (pos < buffer_.size()) {
			const auto open = buffer_.find('<', pos);

			if (open == std::string::npos) {
				break;
			}

			if (buffer_.compare(open, 4, "<!--") == 0) {
				const auto end = buffer_.find("-->", open + 4);

				if (end == std::string::npos) {
					break;
				}

				pos = end + 3;
				continue;
			}

			if (buffer_.compare(open, 9, "<![CDATA[") == 0) {
				const auto end = buffer_.find("]]>", open + 9);

				if (end == std::string::npos) {
					break;
				}

				pos = end + 3;
				continue;
			}

			const auto end = find_tag_end(buffer_, open);

			if (end == std::string::npos) {
				break;
			}

			const bool closing =
				buffer_.compare(open, 2, "</") == 0;

			const bool empty = self_closing(buffer_, open, end);

			if (closing) {
				if (depth == 0) {
					throw std::runtime_error(
						"Unexpected XMPP closing tag");
				}

				--depth;
			} else if (!empty) {
				++depth;
			}

			pos = end + 1;

			if (depth == 0) {
				stanza_end = pos;
				break;
			}
		}

		if (stanza_end == std::string::npos) {
			return;
		}

		std::string_view stanza(buffer_.data(), stanza_end);

		if (mode_ == Mode::Invisible && is_presence(stanza)) {
			output += presence_filter_.process(stanza);
		} else {
			output.append(stanza);
		}

		buffer_.erase(0, stanza_end);
	}
}

} // namespace veil::xmpp