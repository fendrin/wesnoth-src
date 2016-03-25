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

#include "hotkey/hotkey_handler.hpp"

#include "actions/create.hpp"
#include "formula/string_utils.hpp"
#include "game_display.hpp"
#include "game_errors.hpp"
#include "game_events/menu_item.hpp"
#include "game_events/wmi_container.hpp"
#include "game_preferences.hpp"
#include "game_state.hpp"
#include "hotkey/hotkey_command.hpp"
#include "hotkey/hotkey_item.hpp"
#include "network.hpp"		//for nconnections (to determine if we are in a networked game)
#include "play_controller.hpp"
#include "preferences_display.hpp"
#include "savegame.hpp"
#include "saved_game.hpp"
#include "whiteboard/manager.hpp"
#include "wmi_pager.hpp"

#include "units/unit.hpp"

const std::string play_controller::hotkey_handler::wml_menu_hotkey_prefix = "wml_menu:";

play_controller::hotkey_handler::hotkey_handler(play_controller & pc, saved_game & sg)
	: play_controller_(pc)
	, menu_handler_(pc.get_menu_handler())
	, mouse_handler_(pc.get_mouse_handler_base())
	, saved_game_(sg)
	, savenames_()
	, wml_commands_()
	, wml_command_pager_(new wmi_pager())
	, last_context_menu_x_(0)
	, last_context_menu_y_(0)
{}

play_controller::hotkey_handler::~hotkey_handler(){}

game_display * play_controller::hotkey_handler::gui() const {
	return &play_controller_.get_display();
}

game_state & play_controller::hotkey_handler::gamestate() {
	return play_controller_.gamestate();
}

const game_state & play_controller::hotkey_handler::gamestate() const {
	return play_controller_.gamestate();
}

bool play_controller::hotkey_handler::browse() const { return play_controller_.is_browsing(); }
bool play_controller::hotkey_handler::linger() const { return play_controller_.is_lingering(); }

const team & play_controller::hotkey_handler::viewing_team() const { return play_controller_.get_teams_const()[gui()->viewing_team()]; }
bool play_controller::hotkey_handler::viewing_team_is_playing() const { return gui()->viewing_team() == gui()->playing_team(); }

void play_controller::hotkey_handler::objectives(){
	menu_handler_.objectives(gui()->viewing_team()+1);
}

void play_controller::hotkey_handler::show_statistics(){
	menu_handler_.show_statistics(gui()->viewing_team()+1);
}

void play_controller::hotkey_handler::unit_list(){
	menu_handler_.unit_list();
}

void play_controller::hotkey_handler::status_table(){
	menu_handler_.status_table();
}

void play_controller::hotkey_handler::save_game(){
	play_controller_.save_game();
}

void play_controller::hotkey_handler::save_replay(){
	play_controller_.save_replay();
}

void play_controller::hotkey_handler::save_map(){
	play_controller_.save_map();
}

void play_controller::hotkey_handler::load_game(){
	play_controller_.load_game();
}

void play_controller::hotkey_handler::preferences(){
	menu_handler_.preferences();
}

void play_controller::hotkey_handler::left_mouse_click(){
	int x = gui()->get_location_x(gui()->mouseover_hex());
	int y = gui()->get_location_y(gui()->mouseover_hex());

	SDL_MouseButtonEvent event;

	event.button = 1;
	event.x = x + 30;
	event.y = y + 30;
	event.which = 0;
	event.state = SDL_PRESSED;

	mouse_handler_.mouse_press(event, false);
}

void play_controller::hotkey_handler::select_and_action() {
	mouse_handler_.select_or_action(browse());
}

void play_controller::hotkey_handler::move_action(){
	mouse_handler_.move_action(browse());
}

void play_controller::hotkey_handler::deselect_hex(){
	mouse_handler_.deselect_hex();
}
void play_controller::hotkey_handler::select_hex(){
	mouse_handler_.select_hex(gui()->mouseover_hex(), false);
}

void play_controller::hotkey_handler::right_mouse_click(){
	int x = gui()->get_location_x(gui()->mouseover_hex());
	int y = gui()->get_location_y(gui()->mouseover_hex());

	SDL_MouseButtonEvent event;

	event.button = 3;
	event.x = x + 30;
	event.y = y + 30;
	event.which = 0;
	event.state = SDL_PRESSED;

	mouse_handler_.mouse_press(event, true);
}


void play_controller::hotkey_handler::cycle_units(){
	mouse_handler_.cycle_units(browse());
}

void play_controller::hotkey_handler::cycle_back_units(){
	mouse_handler_.cycle_back_units(browse());
}

void play_controller::hotkey_handler::show_chat_log(){
	menu_handler_.show_chat_log();
}

void play_controller::hotkey_handler::show_help(){
	menu_handler_.show_help();
}

void play_controller::hotkey_handler::undo(){
	play_controller_.undo();
}

void play_controller::hotkey_handler::redo(){
	play_controller_.redo();
}

void play_controller::hotkey_handler::show_enemy_moves(bool ignore_units){
	menu_handler_.show_enemy_moves(ignore_units, play_controller_.current_side());
}

void play_controller::hotkey_handler::goto_leader(){
	menu_handler_.goto_leader(play_controller_.current_side());
}

void play_controller::hotkey_handler::unit_description(){
	menu_handler_.unit_description();
}

void play_controller::hotkey_handler::terrain_description(){
	menu_handler_.terrain_description(mouse_handler_);
}

void play_controller::hotkey_handler::toggle_ellipses(){
	menu_handler_.toggle_ellipses();
}

void play_controller::hotkey_handler::toggle_grid(){
	menu_handler_.toggle_grid();
}

void play_controller::hotkey_handler::search(){
	menu_handler_.search();
}

void play_controller::hotkey_handler::toggle_accelerated_speed()
{
	preferences::set_turbo(!preferences::turbo());

	if (preferences::turbo())
	{
		utils::string_map symbols;
		symbols["hk"] = hotkey::get_names(hotkey::hotkey_command::get_command_by_command(hotkey::HOTKEY_ACCELERATED).command);
		gui()->announce(std::string(_("Accelerated speed enabled!")) + "\n" + vgettext("(press $hk to disable)", symbols), font::NORMAL_COLOR);
	}
	else
	{
		gui()->announce(_("Accelerated speed disabled!"), font::NORMAL_COLOR);
	}
}

bool play_controller::hotkey_handler::execute_command(const hotkey::hotkey_command& cmd, int index, hotkey::HOTKEY_EVENT_TYPE type)
{
	hotkey::HOTKEY_COMMAND command = cmd.id;
	if(index >= 0) {
		unsigned i = static_cast<unsigned>(index);
		if(i < savenames_.size() && !savenames_[i].empty()) {
			// Load the game by throwing load_game_exception
			load_autosave(savenames_[i]);

		} else if ( i < wml_commands_.size()  &&  wml_commands_[i] ) {
			if (!wml_command_pager_->capture(*wml_commands_[i])) {
				wml_commands_[i]->fire_event(mouse_handler_.get_last_hex(), gamestate().gamedata_);
			} else { //relaunch the menu
				show_menu(gui()->get_theme().context_menu()->items(),last_context_menu_x_,last_context_menu_y_,true, *gui());
			}
			return true;
		}
	}
	int prefixlen = wml_menu_hotkey_prefix.length();
	if(command == hotkey::HOTKEY_WML && cmd.command.compare(0, prefixlen, wml_menu_hotkey_prefix) == 0)
	{
		std::string name = cmd.command.substr(prefixlen);
		const map_location& hex = mouse_handler_.get_last_hex();

		gamestate().get_wml_menu_items().fire_item(name, hex, gamestate().gamedata_, gamestate(), gamestate().board_.units_);
		/// @todo Shouldn't the function return at this point?
	}
	return command_executor::execute_command(cmd, index, type);
}

bool play_controller::hotkey_handler::can_execute_command(const hotkey::hotkey_command& cmd, int index) const
{
	if(index >= 0) {
		unsigned i = static_cast<unsigned>(index);
		if((i < savenames_.size() && !savenames_[i].empty())
		|| (i < wml_commands_.size() && wml_commands_[i])) {
			return true;
		}
	}
	switch(cmd.id) {

	// Commands we can always do:
	case hotkey::HOTKEY_LEADER:
	case hotkey::HOTKEY_CYCLE_UNITS:
	case hotkey::HOTKEY_CYCLE_BACK_UNITS:
	case hotkey::HOTKEY_ZOOM_IN:
	case hotkey::HOTKEY_ZOOM_OUT:
	case hotkey::HOTKEY_ZOOM_DEFAULT:
	case hotkey::HOTKEY_FULLSCREEN:
	case hotkey::HOTKEY_SCREENSHOT:
	case hotkey::HOTKEY_MAP_SCREENSHOT:
	case hotkey::HOTKEY_ACCELERATED:
	case hotkey::HOTKEY_SAVE_MAP:
	case hotkey::HOTKEY_TOGGLE_ELLIPSES:
	case hotkey::HOTKEY_TOGGLE_GRID:
	case hotkey::HOTKEY_MOUSE_SCROLL:
	case hotkey::HOTKEY_ANIMATE_MAP:
	case hotkey::HOTKEY_STATUS_TABLE:
	case hotkey::HOTKEY_MUTE:
	case hotkey::HOTKEY_PREFERENCES:
	case hotkey::HOTKEY_OBJECTIVES:
	case hotkey::HOTKEY_UNIT_LIST:
	case hotkey::HOTKEY_STATISTICS:
	case hotkey::HOTKEY_QUIT_GAME:
	case hotkey::HOTKEY_QUIT_TO_DESKTOP:
	case hotkey::HOTKEY_SEARCH:
	case hotkey::HOTKEY_HELP:
	case hotkey::HOTKEY_USER_CMD:
	case hotkey::HOTKEY_CUSTOM_CMD:
	case hotkey::HOTKEY_AI_FORMULA:
	case hotkey::HOTKEY_CLEAR_MSG:
	case hotkey::HOTKEY_SELECT_HEX:
	case hotkey::HOTKEY_DESELECT_HEX:
	case hotkey::HOTKEY_MOVE_ACTION:
	case hotkey::HOTKEY_SELECT_AND_ACTION:
	case hotkey::HOTKEY_MINIMAP_CODING_TERRAIN:
	case hotkey::HOTKEY_MINIMAP_CODING_UNIT:
	case hotkey::HOTKEY_MINIMAP_DRAW_UNITS:
	case hotkey::HOTKEY_MINIMAP_DRAW_TERRAIN:
	case hotkey::HOTKEY_MINIMAP_DRAW_VILLAGES:
	case hotkey::HOTKEY_NULL:
	case hotkey::HOTKEY_SAVE_REPLAY:
	case hotkey::HOTKEY_LABEL_SETTINGS:
	case hotkey::LUA_CONSOLE:
		return true;

	// Commands that have some preconditions:
	case hotkey::HOTKEY_SAVE_GAME:
		return !events::commands_disabled;

	case hotkey::HOTKEY_SHOW_ENEMY_MOVES:
	case hotkey::HOTKEY_BEST_ENEMY_MOVES:
		return !linger() && play_controller_.enemies_visible();

	case hotkey::HOTKEY_LOAD_GAME:
		return network::nconnections() == 0; // Can only load games if not in a network game

	case hotkey::HOTKEY_CHAT_LOG:
		return true;

	case hotkey::HOTKEY_REDO:
		return play_controller_.can_redo();
	case hotkey::HOTKEY_UNDO:
		return play_controller_.can_undo();

	case hotkey::HOTKEY_UNIT_DESCRIPTION:
		return menu_handler_.current_unit().valid();

	case hotkey::HOTKEY_TERRAIN_DESCRIPTION:
		return mouse_handler_.get_last_hex().valid();

	case hotkey::HOTKEY_RENAME_UNIT:
		return !events::commands_disabled &&
			menu_handler_.current_unit().valid() &&
			!(menu_handler_.current_unit()->unrenamable()) &&
			menu_handler_.current_unit()->side() == gui()->viewing_side() &&
			play_controller_.get_teams_const()[menu_handler_.current_unit()->side() - 1].is_local_human();

	default:
		return false;
	}
}


static void trim_items(std::vector<std::string>& newitems) {
	if (newitems.size() > 5) {
		std::vector<std::string> subitems;
		subitems.push_back(newitems[0]);
		subitems.push_back(newitems[1]);
		subitems.push_back(newitems[newitems.size() / 3]);
		subitems.push_back(newitems[newitems.size() * 2 / 3]);
		subitems.push_back(newitems.back());
		newitems = subitems;
	}
}

void play_controller::hotkey_handler::expand_autosaves(std::vector<std::string>& items)
{
	const compression::format comp_format =
		preferences::save_compression_format();

	savenames_.clear();
	for (unsigned int i = 0; i < items.size(); ++i) {
		if (items[i] == "AUTOSAVES") {
			items.erase(items.begin() + i);
			std::vector<std::string> newitems;
			std::vector<std::string> newsaves;
			for (unsigned int turn = play_controller_.turn(); turn != 0; turn--) {
				std::string name = saved_game_.classification().label + "-" + _("Auto-Save") + std::to_string(turn);
				if (savegame::save_game_exists(name, comp_format)) {
					newsaves.push_back(
						name + compression::format_extension(comp_format));
					newitems.push_back(_("Back to Turn ") + std::to_string(turn));
				}
			}

			const std::string& start_name = saved_game_.classification().label;
			if(savegame::save_game_exists(start_name, comp_format)) {
				newsaves.push_back(
					start_name + compression::format_extension(comp_format));
				newitems.push_back(_("Back to Start"));
			}

			// Make sure list doesn't get too long: keep top two,
			// midpoint and bottom.
			trim_items(newitems);
			trim_items(newsaves);

			items.insert(items.begin()+i, newitems.begin(), newitems.end());
			savenames_.insert(savenames_.end(), newsaves.begin(), newsaves.end());
			break;
		}
		savenames_.push_back("");
	}
}

void play_controller::hotkey_handler::expand_wml_commands(std::vector<std::string>& items)
{
	wml_commands_.clear();
	for (unsigned int i = 0; i < items.size(); ++i) {
		if (items[i] == "wml") {
			std::vector<std::string> newitems;

			// Replace this placeholder entry with available menu items.
			items.erase(items.begin() + i);
			wml_command_pager_->update_ref(&gamestate().get_wml_menu_items());
			wml_command_pager_->get_items(mouse_handler_.get_last_hex(),
							gamestate().gamedata_, gamestate(), gamestate().board_.units_,
			                                         wml_commands_, newitems);
			items.insert(items.begin()+i, newitems.begin(), newitems.end());
			// End the "for" loop.
			break;
		}
		// Pad the commands with null pointers (keeps the indices of items and
		// wml_commands_ synced).
		wml_commands_.push_back(const_item_ptr());
	}
}

void play_controller::hotkey_handler::show_menu(const std::vector<std::string>& items_arg, int xloc, int yloc, bool context_menu, display& disp)
{
	if (context_menu)
	{
		last_context_menu_x_ = xloc;
		last_context_menu_y_ = yloc;
	}

	std::vector<std::string> items = items_arg;
	const hotkey::hotkey_command* cmd;
	std::vector<std::string>::iterator i = items.begin();
	while(i != items.end()) {
		if (*i == "AUTOSAVES") {
			// Autosave visibility is similar to LOAD_GAME hotkey

			++i; continue; //cmd = &hotkey::hotkey_command::get_command_by_command(hotkey::HOTKEY_LOAD_GAME);
		} else {
			cmd = &hotkey::get_hotkey_command(*i);
		}
		// Remove commands that can't be executed or don't belong in this type of menu
		if(*i != "wml" && (!can_execute_command(*cmd) || (context_menu && !in_context_menu(cmd->id)))) {
			i = items.erase(i);
			continue;
		}
		++i;
	}

	// Add special non-hotkey items to the menu and remember their indices
	expand_autosaves(items);
	expand_wml_commands(items);

	if(items.empty())
		return;

	command_executor::show_menu(items, xloc, yloc, context_menu, disp);
}

bool play_controller::hotkey_handler::in_context_menu(hotkey::HOTKEY_COMMAND command) const
{
	switch(command) {
	// Only display these if the mouse is over a castle or keep tile
	case hotkey::HOTKEY_RECRUIT:
	case hotkey::HOTKEY_REPEAT_RECRUIT:
	case hotkey::HOTKEY_RECALL: {
		// last_hex_ is set by mouse_events::mouse_motion
		const map_location & last_hex = mouse_handler_.get_last_hex();
		const int viewing_side = gui()->viewing_side();

		// A quick check to save us having to create the future map and
		// possibly loop through all units.
		if ( !play_controller_.get_map_const().is_keep(last_hex)  &&
		     !play_controller_.get_map_const().is_castle(last_hex) )
			return false;

		wb::future_map future; /* lasts until method returns. */

		return gamestate().side_can_recruit_on(viewing_side, last_hex);
	}
	default:
		return true;
	}
}

std::string play_controller::hotkey_handler::get_action_image(hotkey::HOTKEY_COMMAND command, int index) const
{
	if(index >= 0 && index < static_cast<int>(wml_commands_.size())) {
		const const_item_ptr wmi = wml_commands_[index];
		if ( wmi ) {
			return wmi->image();
		}
	}
	return command_executor::get_action_image(command, index);
}

hotkey::ACTION_STATE play_controller::hotkey_handler::get_action_state(hotkey::HOTKEY_COMMAND command, int /*index*/) const
{
	switch(command) {

	case hotkey::HOTKEY_MINIMAP_DRAW_VILLAGES:
		return (preferences::minimap_draw_villages()) ? hotkey::ACTION_ON : hotkey::ACTION_OFF;
	case hotkey::HOTKEY_MINIMAP_CODING_UNIT:
		return (preferences::minimap_movement_coding()) ? hotkey::ACTION_ON : hotkey::ACTION_OFF;
	case hotkey::HOTKEY_MINIMAP_CODING_TERRAIN:
		return (preferences::minimap_terrain_coding()) ? hotkey::ACTION_ON : hotkey::ACTION_OFF;
	case hotkey::HOTKEY_MINIMAP_DRAW_UNITS:
		return (preferences::minimap_draw_units()) ? hotkey::ACTION_ON : hotkey::ACTION_OFF;
	case hotkey::HOTKEY_MINIMAP_DRAW_TERRAIN:
		return (preferences::minimap_draw_terrain()) ? hotkey::ACTION_ON : hotkey::ACTION_OFF;
	case hotkey::HOTKEY_ZOOM_DEFAULT:
		return (gui()->get_zoom_factor() == 1.0) ? hotkey::ACTION_ON : hotkey::ACTION_OFF;
	case hotkey::HOTKEY_DELAY_SHROUD:
		return viewing_team().auto_shroud_updates() ? hotkey::ACTION_OFF : hotkey::ACTION_ON;
	default:
		return hotkey::ACTION_STATELESS;
	}
}

void play_controller::hotkey_handler::load_autosave(const std::string& filename)
{
	throw game::load_game_exception(filename, false, false, false, "", true);
}
