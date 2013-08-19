/*
	Copyright (C) 2013 by Pierre Talbot <ptalbot@mopong.net>
	Part of the Battle for Wesnoth Project http://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

#ifndef UMCD_PROTOCOL_INFO_HPP
#define UMCD_PROTOCOL_INFO_HPP

#include <cstddef>

namespace umcd{
class environment_loader;

class protocol_info
{
public:
	std::size_t header_max_size() const;

protected:
	friend class environment_loader;
	void set_header_max_size(std::size_t header_size);

private:
	static std::size_t header_max_size_;
};

} // namespace umcd
#endif // UMCD_PROTOCOL_INFO_HPP