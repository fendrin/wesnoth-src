/*
   Copyright (C) 2008 - 2013 by Fabian Mueller <fabianmueller5@gmx.de>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-editor"

#include "mouse_action_unit.hpp"
#include "../action_unit.hpp"

#include "../../editor_display.hpp"
#include "gui/dialogs/unit_create.hpp"
#include "tooltips.hpp"

#include "map_location.hpp"

namespace editor {


void mouse_action_unit::move(editor_display& disp, const map_location& hex)
{
	if (hex != previous_move_hex_) {

		update_brush_highlights(disp, hex);

		std::set<map_location> adjacent_set;
		map_location adjacent[6];
		get_adjacent_tiles(previous_move_hex_, adjacent);

		for (int i = 0; i < 6; i++)
			adjacent_set.insert(adjacent[i]);

		disp.invalidate(adjacent_set);
		previous_move_hex_ = hex;

		const unit_map& units = disp.get_units();
		const unit_map::const_unit_iterator unit_it = units.find(hex);
		if (unit_it != units.end()) {

			SDL_Rect rect;
			rect.x = disp.get_location_x(hex);
			rect.y = disp.get_location_y(hex);
			rect.h = 80;
			rect.w = 80;
			std::stringstream str;
			str << unit_it->id() << "\n" << unit_it->name() << "\n" << unit_it->type_name();
			tooltips::clear_tooltips();
			tooltips::add_tooltip(rect, str.str());
		}
	}
}

editor_action* mouse_action_unit::click_left(editor_display& disp, int x, int y)
{
	start_hex_ = disp.hex_clicked_on(x, y);
	if (!disp.get_map().on_board(start_hex_)) {
		return NULL;
	}

	const unit_map& units = disp.get_units();
	const unit_map::const_unit_iterator unit_it = units.find(start_hex_);
	if (unit_it != units.end())
		set_unit_mouse_overlay(disp, unit_it->type());

	click_ = true;
	return NULL;
}

editor_action* mouse_action_unit::drag_left(editor_display& disp, int x, int y, bool& /*partial*/, editor_action* /*last_undo*/)
{
	map_location hex = disp.hex_clicked_on(x, y);
	click_ = (hex == start_hex_);
	return NULL;
}

editor_action* mouse_action_unit::up_left(editor_display& disp, int x, int y)
{
	if (!click_) return NULL;
	click_ = false;
	map_location hex = disp.hex_clicked_on(x, y);
	if (!disp.get_map().on_board(hex)) {
		return NULL;
	}

	unit_type type = unit_palette_.selected_fg_item();

	// Does this serve a purpose other than making sure the type is built?
	// (Calling unit_types.build_unit_type(type) would now accomplish that
	// with less overhead.)
	const std::string& type_id = type.id();
	const unit_type *new_unit_type = unit_types.find(type_id);
	if (!new_unit_type) {
		//TODO rewrite the error message.
		ERR_ED << "create unit dialog returned inexistent or unusable unit_type id '" << type_id << "'\n";
		return NULL;
	}

	const unit_type &ut = *new_unit_type;
	unit_race::GENDER gender = ut.genders().front();

	unit new_unit(ut, disp.viewing_side(), true, gender);
	editor_action* action = new editor_action_unit(hex, new_unit);
	return action;
}

editor_action* mouse_action_unit::drag_end_left(editor_display& disp, int x, int y)
{
	if (click_) return NULL;
	editor_action* action = NULL;

	map_location hex = disp.hex_clicked_on(x, y);
	if (!disp.get_map().on_board(hex))
		return NULL;

	const unit_map& units = disp.get_units();
	const unit_map::const_unit_iterator unit_it = units.find(start_hex_);
	if (unit_it == units.end())
		return NULL;

	action = new editor_action_unit_replace(start_hex_, hex);
	return action;
}

/*
editor_action* mouse_action_unit::click_right(editor_display& disp, int x, int y)
{
	map_location hex = disp.hex_clicked_on(x, y);
	start_hex_ = hex;
	previous_move_hex_ = hex;

	const unit_map& units = disp.get_units();
	const unit_map::const_unit_iterator unit_it = units.find(start_hex_);

	if (unit_it != units.end()) {
		old_direction_ = unit_it->facing();
	}

	click_ = true;
	return NULL;
}
*/

//editor_action* mouse_action_unit::drag_right(editor_display& disp, int x, int y, bool& /*partial*/, editor_action* /*last_undo*/)
//{
//	map_location hex = disp.hex_clicked_on(x, y);
//	if (previous_move_hex_ == hex)
//		return NULL;
//
//	click_ = (start_hex_ == hex);
//	previous_move_hex_ = hex;
//
//	const unit_map& units = disp.get_units();
//
//	const unit_map::const_unit_iterator unit_it = units.find(start_hex_);
//	if (unit_it != units.end()) {
//		for (map_location::DIRECTION new_direction = map_location::NORTH;
//				new_direction <= map_location::NORTH_WEST;
//				new_direction = map_location::DIRECTION(new_direction +1)){
//			if (unit_it->get_location().get_direction(new_direction, 1) == hex) {
//				return new editor_action_unit_facing(start_hex_, new_direction, old_direction_);
//			}
//		}
//	}
//
//	return NULL;
//}

//editor_action* mouse_action_unit::up_right(editor_display& disp, int /*x*/, int /*y*/)
//{
//	if (!click_) return NULL;
//	click_ = false;
//
//	const unit_map& units = disp.get_units();
//	const unit_map::const_unit_iterator unit_it = units.find(start_hex_);
//	if (unit_it != units.end()) {
//		return new editor_action_unit_delete(start_hex_);
//	}
//
//	return NULL;
//}

//editor_action* mouse_action_unit::drag_end_right(editor_display& disp, int x, int y)
//{
//	if (click_) return NULL;
//
//	map_location hex = disp.hex_clicked_on(x, y);
//	if (!disp.get_map().on_board(hex))
//		return NULL;
//
//	if(new_direction_ != old_direction_) {
//
//	const unit_map& units = disp.get_units();
//	const unit_map::const_unit_iterator unit_it = units.find(start_hex_);
//		if (unit_it != units.end()) {
//			return new editor_action_unit_facing(start_hex_, new_direction_, old_direction_);
//		}
//	}
//
//	return NULL;
//}


void mouse_action_unit::set_mouse_overlay(editor_display& disp)
{
	const unit_type& u = unit_palette_.selected_fg_item();
	set_unit_mouse_overlay(disp, u);
}

void mouse_action_unit::set_unit_mouse_overlay(editor_display& disp, const unit_type& u)
{

	std::stringstream filename;
		filename << u.image() << "~RC(" << u.flag_rgb() << '>'
		    	 << team::get_side_color_index(disp.viewing_side()) << ')';

	surface image(image::get_image(filename.str()));
	Uint8 alpha = 196;
	//TODO don't hardcode
	int size = 72;
	//int size = image->w;
	int zoom = static_cast<int>(size * disp.get_zoom_factor());

	// Add the alpha factor and scale the image
	image = scale_surface(adjust_surface_alpha(image, alpha), zoom, zoom);
	disp.set_mouseover_hex_overlay(image);
}


} //end namespace editor
