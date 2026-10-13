#ifndef VEIL_XMPP_PRESENCE_FILTER_HPP
#define VEIL_XMPP_PRESENCE_FILTER_HPP

#include <string>
#include <string_view>

namespace veil::xmpp
{

class presence_filter
{
public:
	explicit presence_filter(bool invisible = true,
				 bool allow_directed_presence = true);

	std::string process(std::string_view stanza) const;

private:
	bool invisible_;
	bool allow_directed_presence_;
};

} // namespace veil::xmpp

#endif