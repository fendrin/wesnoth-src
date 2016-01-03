/*
   Copyright (C) 2014 - 2016 by Chris Beck <render787@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LUA_TEAM_HPP_INCLUDED
#define LUA_TEAM_HPP_INCLUDED

class team;
struct lua_State;

#include <string>

/**
 * This namespace contains bindings for lua to hold a pointer to a team,
 * and to access and modify it.
 */
namespace lua_team {
	std::string register_metatable(lua_State *);
} //end namespace lua_team

// Create a full userdata containing a pointer to the team.
void luaW_pushteam(lua_State *, team &);

#endif
