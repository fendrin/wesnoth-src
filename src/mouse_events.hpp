/*
   Copyright (C) 2006 - 2014 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef MOUSE_EVENTS_H_INCLUDED
#define MOUSE_EVENTS_H_INCLUDED

#include "game_display.hpp"             // for game_display
#include "map_location.hpp"             // for map_location
#include "mouse_handler_base.hpp"       // for mouse_handler_base
#include "pathfind/pathfind.hpp"        // for marked_route, paths
#include "unit_map.hpp"                 // for unit_map, etc

#include <set>                          // for set
#include <vector>                       // for vector
#include "SDL_events.h"                 // for SDL_MouseButtonEvent

class battle_context;  // lines 23-23
class game_board;
class team;
class unit;

namespace events{

class mouse_handler : public mouse_handler_base {
public:
	mouse_handler(game_display* gui, game_board & board);
	~mouse_handler();
	static mouse_handler* get_singleton() { return singleton_ ;}
	void set_side(int side_number);
	void mouse_press(const SDL_MouseButtonEvent& event, const bool browse);
	void cycle_units(const bool browse, const bool reverse = false);
	void cycle_back_units(const bool browse) { cycle_units(browse, true); }

	int get_path_turns() const { return path_turns_; }

	/**
	 * @param loc the location occupied by the enemy
	 * @returns the location from which the selected unit can attack the enemy
	 */
	map_location current_unit_attacks_from(const map_location& loc) const;
	const pathfind::paths& current_paths() const { return current_paths_; }

	const map_location& get_last_hex() const { return last_hex_; }
	map_location get_selected_hex() const { return selected_hex_; }
	void set_path_turns(const int path_turns) { path_turns_ = path_turns; }
	void set_current_paths(const pathfind::paths & new_paths);
	void deselect_hex();
	void invalidate_reachmap() { reachmap_invalid_ = true; }

	void set_gui(game_display* gui) { gui_ = gui; }

	unit_map::iterator selected_unit();

	pathfind::marked_route get_route(const unit* un, map_location go_to, team &team) const;

	const pathfind::marked_route& get_current_route() const { return current_route_; }

	//get visible adjacent enemies of 1-based side around location loc
	std::set<map_location> get_adj_enemies(const map_location& loc, int side) const;

	// show the attack dialog and return the choice made
	// which can be invalid if 'cancel' was used
	int show_attack_dialog(const map_location& attacker_loc, const map_location& defender_loc);
	// wrapper to catch bad_alloc so this should be called
	void attack_enemy(const map_location& attacker_loc, const map_location& defender_loc, int choice);

	/// Moves a unit across the board for a player.
	size_t move_unit_along_route(const std::vector<map_location> & steps, bool & interrupted);

	void select_hex(const map_location& hex, const bool browse,
		const bool highlight = true,
		const bool fire_event = true);

	void move_action(bool browse);

	void select_or_action(bool browse);

	void left_mouse_up(int x, int y, const bool /*browse*/);
	void mouse_wheel_up(int x, int y, const bool /*browse*/);
	void mouse_wheel_down(int x, int y, const bool /*browse*/);
	void mouse_wheel_left(int x, int y, const bool /*browse*/);
	void mouse_wheel_right(int x, int y, const bool /*browse*/);

protected:
	/**
	 * Due to the way this class is constructed we can assume that the
	 * display* gui_ member actually points to a game_display (derived class)
	 */
	game_display& gui() { return *gui_; }
	/** Const version */
	const game_display& gui() const { return *gui_; }

	int drag_threshold() const;
	/**
	 * Use update to force an update of the mouse state.
	 */
	void mouse_motion(int x, int y, const bool browse, bool update=false, map_location loc = map_location::null_location());
	bool right_click_show_menu(int x, int y, const bool browse);
//	bool left_click(int x, int y, const bool browse);
	bool move_unit_along_current_route();

	void save_whiteboard_attack(const map_location& attacker_loc, const map_location& defender_loc, int weapon_choice);

	// fill weapon choices into bc_vector
	// return the best weapon choice
	int fill_weapon_choices(std::vector<battle_context>& bc_vector, unit_map::iterator attacker, unit_map::iterator defender);
	// the real function but can throw bad_alloc
	// choice is the attack chosen in the attack dialog
	void attack_enemy_(const map_location& attacker_loc
			, const map_location& defender_loc
			, int choice);

	void show_attack_options(const unit_map::const_iterator &u);
	unit_map::const_iterator find_unit(const map_location& hex) const;
	unit_map::iterator find_unit(const map_location& hex);
	bool unit_in_cycle(unit_map::const_iterator it);
private:
	team& viewing_team();
	const team& viewing_team() const;
	team &current_team();

	game_display* gui_;
	game_board & board_;

	// previous highlighted hexes
	// the hex of the selected unit and empty hex are "free"
	map_location previous_hex_;
	map_location previous_free_hex_;
	map_location selected_hex_;
	map_location next_unit_;
	pathfind::marked_route current_route_;
	pathfind::paths current_paths_;
	bool unselected_paths_;
	int path_turns_;
	int side_num_;

	bool over_route_;
	bool reachmap_invalid_;
	bool show_partial_move_;

	static mouse_handler * singleton_;
};

}

#endif
