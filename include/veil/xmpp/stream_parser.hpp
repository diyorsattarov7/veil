#ifndef VEIL_XMPP_STREAM_PARSER_HPP
#define VEIL_XMPP_STREAM_PARSER_HPP

#include <veil/xmpp/presence_filter.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace veil::xmpp {

class StreamParser {
public:
    enum class Mode {
        Online,
        Invisible
    };

    explicit StreamParser(Mode mode = Mode::Invisible);

    std::string feed(std::string_view data);

    void reset();
    void set_mode(Mode mode);

private:
    void process(std::string& output);

    static constexpr std::size_t max_buffer_size = 1024 * 1024;

    Mode mode_;
    PresenceFilter presence_filter_;
    std::string buffer_;
};

} // namespace veil::xmpp

#endif // VEIL_XMPP_STREAM_PARSER_HPP
