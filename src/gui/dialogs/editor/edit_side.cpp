/*
   Copyright (C) 2010 - 2016 by Fabian Müller <fabianmueller5@gmx.de>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/editor/edit_side.hpp"

#include "gui/auxiliary/field.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/settings.hpp"

#include "utils/functional.hpp"

namespace gui2
{

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_edit_side
 *
 * == Edit side ==
 *
 * Dialog for editing gamemap sides.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * id & & text_box & m &
 *         Input field for the id. $
 *
 * team_only_toggle & & toggle_button & m &
 *         Checkbox for whether to make the label visible to the player's team
 *         only or not. $
 *
 * @end{table}
 */

REGISTER_DIALOG(editor_edit_side)

teditor_edit_side::teditor_edit_side(int side,
									 std::string& id,
									 std::string& name,
									 int& gold,
									 int& income,
									 int& village_income,
									 int& village_support,
									 bool& fog,
									 bool& shroud,
									 team::SHARE_VISION& share_vision,
									 team::CONTROLLER& controller,
									 bool& no_leader,
									 bool& hidden)
	: controller_(controller)
	, share_vision_(share_vision)
{
	std::stringstream side_stream;
	side_stream << side;
	register_label("side_number", true, side_stream.str(), true);

	register_text("team_name", true, id, true);
	register_text("user_team_name", true, name, true);

	register_integer("gold", true, gold);
	register_integer("income", true, income);

	register_integer("village_income", true, village_income);
	register_integer("village_support", true, village_support);

	register_bool("fog", true, fog);
	register_bool("shroud", true, shroud);

	register_bool("no_leader", true, no_leader);
	register_bool("hidden", true, hidden);
}

void teditor_edit_side::pre_show(twindow& window)
{
	controller_group.add_member(&find_widget<ttoggle_button>(&window, "controller_human", false), team::CONTROLLER::HUMAN);
	controller_group.add_member(&find_widget<ttoggle_button>(&window, "controller_ai", false),    team::CONTROLLER::AI);
	controller_group.add_member(&find_widget<ttoggle_button>(&window, "controller_null", false),  team::CONTROLLER::EMPTY);

	controller_group.set_member_states(controller_);

	vision_group.add_member(&find_widget<ttoggle_button>(&window, "vision_all", false),    team::SHARE_VISION::ALL);
	vision_group.add_member(&find_widget<ttoggle_button>(&window, "vision_shroud", false), team::SHARE_VISION::SHROUD);
	vision_group.add_member(&find_widget<ttoggle_button>(&window, "vision_null", false),   team::SHARE_VISION::NONE);

	vision_group.set_member_states(share_vision_);
}

void teditor_edit_side::post_show(twindow&)
{
	controller_ = controller_group.get_active_member_value();
	share_vision_ = vision_group.get_active_member_value();
}

} // end namespace gui2
