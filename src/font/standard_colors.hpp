/*
 * Copyright (C) 2008 - 2016 by Mark de Wever <koraq@xs4all.nl>
 * Part of the Battle for Wesnoth Project http://www.wesnoth.org/
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY.
 * 
 * See the COPYING file for more details.
 */

#ifndef FONT_STANDARD_COLORS_HPP
#define FONT_STANDARD_COLORS_HPP

#include <SDL.h>

namespace font {
	extern const SDL_Color NORMAL_COLOR, GRAY_COLOR, LOBBY_COLOR, GOOD_COLOR, BAD_COLOR,
		BLACK_COLOR, YELLOW_COLOR, BUTTON_COLOR, BIGMAP_COLOR,
		PETRIFIED_COLOR, TITLE_COLOR, DISABLED_COLOR, LABEL_COLOR;
}

#endif