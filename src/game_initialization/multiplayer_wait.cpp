/*
   Copyright (C) 2007 - 2016 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "construct_dialog.hpp"
#include "gettext.hpp"
#include "game_config_manager.hpp"
#include "game_preferences.hpp"
#include "gui/dialogs/multiplayer/faction_select.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/network_transmission.hpp"
#include "gui/widgets/window.hpp"
#include "image.hpp"
#include "log.hpp"
#include "font/marked-up_text.hpp"
#include "font/standard_colors.hpp"
#include "mp_game_utils.hpp"
#include "multiplayer_wait.hpp"
#include "statistics.hpp"
#include "saved_game.hpp"
#include "mp_ui_alerts.hpp"
#include "scripting/plugins/context.hpp"
#include "sdl/rect.hpp"
#include "units/types.hpp"
#include "wml_exception.hpp"
#include "wml_separators.hpp"
#include "formula/string_utils.hpp"
#include "video.hpp"

#include "utils/functional.hpp"

static lg::log_domain log_network("network");
#define DBG_NW LOG_STREAM(debug, log_network)
#define LOG_NW LOG_STREAM(info, log_network)

static lg::log_domain log_enginerefac("enginerefac");
#define LOG_RG LOG_STREAM(info, log_enginerefac)
#define ERR_RG LOG_STREAM(err, log_enginerefac)

static lg::log_domain log_mp("mp/main");
#define DBG_MP LOG_STREAM(debug, log_mp)
#define ERR_MP LOG_STREAM(err, log_mp)


namespace mp {

wait::wait(CVideo& v, twesnothd_connection* wesnothd_connection, const config& cfg, saved_game& state,
	mp::chat& c, config& gamelist, const bool first_scenario) :
	ui(v, wesnothd_connection, _("Game Lobby"), cfg, c, gamelist),
	cancel_button_(video(), first_scenario ? _("Cancel") : _("Quit")),
	start_label_(video(), _("Waiting for game to start..."), font::SIZE_SMALL, font::LOBBY_COLOR),
	game_menu_(video(), std::vector<std::string>(), false, -1, -1, nullptr, &gui::menu::bluebg_style),
	level_(),
	state_(state),
	first_scenario_(first_scenario),
	stop_updates_(false)
{
	game_menu_.set_numeric_keypress_selection(false);
	gamelist_updated();

	plugins_context_.reset(new plugins_context("Multiplayer Wait"));

	//These structure initializers create a lobby::process_data_event
	plugins_context_->set_callback("quit", 		std::bind(&wait::process_event_impl, this, true), 						false);
	plugins_context_->set_callback("chat", [this](const config& cfg) { send_chat_message(cfg["message"], false); }, true);
}

wait::~wait()
{
	try {
		if (get_result() == QUIT) {
			state_ = saved_game();
			state_.classification().campaign_type = game_classification::CAMPAIGN_TYPE::MULTIPLAYER;
		}
	} catch (...) {}
}

void wait::process_event()
{
	process_event_impl(cancel_button_.pressed());
}

void wait::process_event_impl(bool quit)
{
	if (quit) {
		set_result(QUIT);
	}
}

void wait::join_game(bool observe)
{
	const bool download_res = download_level_data();
	if (!download_res) {
		DBG_MP << "mp wait: could not download level data, quitting...";
		set_result(QUIT);
		return;
	} else if (level_["started"].to_bool()) {
		set_result(PLAY);
		return;
	}

	if (first_scenario_) {
		state_ = saved_game();
		state_.classification() = game_classification(level_);

		if(state_.classification().campaign_type != game_classification::CAMPAIGN_TYPE::MULTIPLAYER) {
			ERR_MP << "Mp wait recieved a game that is not a multiplayer game\n";
		}
		// Make sure that we have the same config as host, if possible.
		game_config_manager::get()->load_game_config_for_game(state_.classification());
	}

	// Add the map name to the title.
	append_to_title(": " + get_scenario()["name"].t_str());

	game_config::add_color_info(get_scenario());
	if (!observe) {
		//search for an appropriate vacant slot. If a description is set
		//(i.e. we're loading from a saved game), then prefer to get the side
		//with the same description as our login. Otherwise just choose the first
		//available side.
		const config *side_choice = nullptr;
		int side_num = -1, nb_sides = 0;
		for (const config &sd : get_scenario().child_range("side"))
		{
			DBG_MP << "*** side " << nb_sides << "***\n" << sd.debug() << "***\n";

			if (sd["controller"] == "reserved" && sd["current_player"] == preferences::login())
			{
				side_choice = &sd;
				side_num = nb_sides;
				break;
			}
			if (sd["controller"] == "human" && sd["player_id"].empty())
			{
				if (!side_choice) { // found the first empty side
					side_choice = &sd;
					side_num = nb_sides;
				}
				if (sd["current_player"] == preferences::login()) {
					side_choice = &sd;
					side_num = nb_sides;
					break;  // found the preferred one
				}
			}
			if (sd["player_id"] == preferences::login())
			{
				//We already own a side in this game.
				generate_menu();
				return;
			}
			++nb_sides;
		}
		if (!side_choice) {
			DBG_MP << "could not find a side, all " << get_scenario().child_count("side") << " sides were unsuitable\n";
			set_result(QUIT);
			return;
		}

		bool allow_changes = (*side_choice)["allow_changes"].to_bool(true);

		//if the client is allowed to choose their team, instead of having
		//it set by the server, do that here.
		if(allow_changes) {
			events::event_context context;

			const config &era = level_.child("era");
			/** @todo Check whether we have the era. If we don't inform the user. */
			if (!era)
				throw config::error(_("No era information found."));
			config::const_child_itors possible_sides = era.child_range("multiplayer_side");
			if (possible_sides.empty()) {
				set_result(QUIT);
				throw config::error(_("No multiplayer sides found"));
			}

			const std::string color = (*side_choice)["color"].str();

			std::vector<const config*> era_factions;
			for (const config &side : possible_sides) {
				era_factions.push_back(&side);
			}

			const bool is_mp = state_.classification().is_normal_mp_game();
			const bool lock_settings =
				get_scenario()["force_lock_settings"].to_bool(!is_mp);
			const bool use_map_settings =
				level_.child("multiplayer")["mp_use_map_settings"].to_bool();
			const bool saved_game =
				level_.child("multiplayer")["savegame"].to_bool();

			ng::flg_manager flg(era_factions, *side_choice, lock_settings, use_map_settings,
				saved_game);

			std::vector<std::string> choices;
			for (const config *s : flg.choosable_factions())
			{
				const config &side = *s;
				const std::string &name = side["name"];
				const std::string &icon = side["image"];

				if (!icon.empty()) {
					std::string rgb = side["flag_rgb"];
					if (rgb.empty())
						rgb = "magenta";

					choices.push_back(IMAGE_PREFIX + icon + "~RC(" + rgb + ">" +
						color + ")" + COLUMN_SEPARATOR + name);
				} else {
					choices.push_back(name);
				}
			}

			gui2::tfaction_select dlg(flg, color, side_num + 1);
			dlg.show(video());

			if(dlg.get_retval() != gui2::twindow::OK) {
				set_result(QUIT);
				return;
			}

			config faction;
			config& change = faction.add_child("change_faction");
			change["change_faction"] = true;
			change["name"] = preferences::login();
			change["faction"] = flg.current_faction()["id"];
			change["leader"] = flg.current_leader();
			change["gender"] = flg.current_gender();
			send_to_server(faction);
		}

	}

	generate_menu();
}

void wait::start_game()
{
	if (const config &stats = level_.child("statistics")) {
		statistics::fresh_stats();
		statistics::read_stats(stats);
	}
	level_to_gamestate(level_, state_);

	LOG_NW << "starting game\n";
	mp_ui_alerts::game_has_begun();
}

void wait::layout_children(const SDL_Rect& rect)
{
	ui::layout_children(rect);

	const SDL_Rect ca = client_area();
	int y = ca.y + ca.h - cancel_button_.height();

	game_menu_.set_location(ca.x, ca.y + title().height());
	game_menu_.set_measurements(ca.w, y - ca.y - title().height()
			- gui::ButtonVPadding);
	game_menu_.set_max_width(ca.w);
	game_menu_.set_max_height(y - ca.y - title().height() - gui::ButtonVPadding);
	cancel_button_.set_location(ca.x + ca.w - cancel_button_.width(), y);
	start_label_.set_location(ca.x, y + 4);
}

void wait::hide_children(bool hide)
{
	ui::hide_children(hide);

	cancel_button_.hide(hide);
	game_menu_.hide(hide);
}

void wait::process_network_data(const config& data)
{
	ui::process_network_data(data);

	if(!data["message"].empty()) {
		gui2::show_transient_message(video()
				, _("Response")
				, data["message"]);
	}
	if (data["failed"].to_bool()) {
		set_result(QUIT);
		return;
	} else if(data.child("stop_updates")) {
		stop_updates_ = true;
	} else if(data.child("start_game")) {
		LOG_NW << "received start_game message\n";
		set_result(PLAY);
		return;
	} else if(data.child("leave_game")) {
		set_result(QUIT);
		return;
	} else if (const config &c = data.child("scenario_diff")) {
		LOG_NW << "received diff for scenario... applying...\n";
		/** @todo We should catch config::error and then leave the game. */
		level_.apply_diff(c);
		generate_menu();
	} else if(const config &change = data.child("change_controller")) {
		LOG_NW << "received change controller" << std::endl;
		LOG_RG << "multiplayer_wait: [change_controller]" << std::endl;
		LOG_RG << data.debug() << std::endl;
		//const int side = std::stoi(change["side"]);

		if (config & sidetochange = get_scenario().find_child("side", "side", change["side"])) {
			LOG_RG << "found side : " << sidetochange.debug() << std::endl;
			sidetochange.merge_with(change);
			LOG_RG << "changed to : " << sidetochange.debug() << std::endl;
		} else {
			LOG_RG << "change_controller didn't find any side!" << std::endl;
		}
	} else if(data.has_child("scenario") || data.has_child("snapshot") || data.child("next_scenario")) {
		level_ = first_scenario_ ? data : data.child("next_scenario");
		LOG_NW << "got some sides. Current number of sides = "
			<< get_scenario().child_count("side") << ','
			<< data.child_count("side") << '\n';
		generate_menu();
	}
}

static std::string generate_user_description(const config& side)
{
	//allow the host to overwrite it, this is needed because only the host knows the ai_algorithm.
	if(const config::attribute_value* desc = side.get("user_description")) {
		return desc->str();
	}

	std::string controller_type = side["controller"].str();
	std::string reservation = side["reserved_for"].str();
	std::string owner = side["player_id"].str();

	if(controller_type == "ai") {
		return _("Computer Player");
	}
	else if (controller_type == "null") {
		return _("(Empty slot)");
	}
	else if (controller_type == "reserved") {
		utils::string_map symbols;
		symbols["playername"] = reservation;
		return vgettext("(Reserved for $playername)",symbols);
	}
	else if(owner.empty()) {
		return _("(Vacant slot)");
	}
	else if (controller_type == "human" || controller_type == "network") {
		return owner;
	}
	else {
		ERR_RG << "Found unknown controller type:" << controller_type << std::endl;
		return _("(empty)");
	}
}

void wait::generate_menu()
{
	if (stop_updates_)
		return;

	std::vector<std::string> details;
	std::set<std::string> playerlist;

	for (const config &sd : get_scenario().child_range("side"))
	{
		if (!sd["allow_player"].to_bool(true)) {
			continue;
		}

		std::string description = generate_user_description(sd);

		t_string side_name = sd["faction_name"];
		std::string leader_type = sd["type"];
		std::string gender_id = sd["gender"];

		// Hack: if there is a unit which can recruit, use it as a
		// leader. Necessary to display leader information when loading
		// saves.
		for (const config &side_unit : sd.child_range("unit"))
		{
			if (side_unit["canrecruit"].to_bool()) {
				leader_type = side_unit["type"].str();
				break;
			}
		}

		if(!sd["player_id"].empty())
			playerlist.insert(sd["player_id"]);

		std::string leader_name;
		std::string leader_image;

		const unit_type *ut = unit_types.find(leader_type);

		if (ut) {
			const unit_type &utg = ut->get_gender_unit_type(gender_id);

			leader_name = utg.type_name();
#ifdef LOW_MEM
			leader_image = utg.image();
#else
			std::string RCcolor = sd["color"];

			if (RCcolor.empty())
				RCcolor = sd["side"].str();
			leader_image = utg.image() + std::string("~RC(") + utg.flag_rgb() + ">" + RCcolor + ")";
#endif
		} else {
			leader_image = ng::random_enemy_picture;
		}
		if (!leader_image.empty()) {
			// Dumps the "image" part of the faction name, if any,
			// to replace it by a picture of the actual leader
			if(side_name.str()[0] == font::IMAGE) {
				std::string::size_type p =
					side_name.str().find_first_of(COLUMN_SEPARATOR);
				if(p != std::string::npos && p < side_name.size()) {
					side_name = IMAGE_PREFIX + leader_image + COLUMN_SEPARATOR + side_name.str().substr(p+1);
				}
			} else {
				// no image prefix, just add the leader image
				// (assuming that there is also no COLUMN_SEPARATOR)
				side_name = IMAGE_PREFIX + leader_image + COLUMN_SEPARATOR + side_name.str();
			}
		}

		std::stringstream str;
		str << sd["side"] << ". " << COLUMN_SEPARATOR;
		str << description << COLUMN_SEPARATOR << side_name << COLUMN_SEPARATOR;
		// Mark parentheses translatable for languages like Japanese
		if(!leader_name.empty())
			str << _("(") << leader_name << _(")");
		str << COLUMN_SEPARATOR;
		// Don't show gold for saved games
		if (sd["allow_changes"].to_bool())
			str << sd["gold"] << ' ' << _n("multiplayer_starting_gold^Gold", "multiplayer_starting_gold^Gold", sd["gold"].to_int()) << COLUMN_SEPARATOR;

		int income_amt = sd["income"];
		if(income_amt != 0){
			str << _("(") << _("Income") << ' ';
			if(income_amt > 0)
				str << _("+");
			str << sd["income"] << _(")");
		}

		str << COLUMN_SEPARATOR << t_string::from_serialized(sd["user_team_name"].str());

		str << COLUMN_SEPARATOR << get_color_string(sd["color"].str());
		details.push_back(str.str());
	}

	game_menu_.set_items(details);

	// Uses the actual connected player list if we do not have any
	// "gamelist" user data
	if (!gamelist().child("user")) {
		set_user_list(std::vector<std::string>(playerlist.begin(), playerlist.end()), true);
	}
}

bool wait::download_level_data()
{
	assert(wesnothd_connection_);
	DBG_MP << "download_level_data()\n";
	if (!first_scenario_) {
		// Ask for the next scenario data.
		send_to_server(config("load_next_scenario"));
	}
	bool has_scenario_and_controllers = false;
	while (!has_scenario_and_controllers) {
		config revc;
		bool data_res = gui2::tnetwork_transmission::wesnothd_receive_dialog(
			video(), "download level data", revc, *wesnothd_connection_);

		if (!data_res) {
			DBG_MP << "download_level_data bad results\n";
			return false;
		}
		check_response(data_res, revc);
		if (revc.child("leave_game")) {
			return false;
		}
		else if(config& next_scenario = revc.child("next_scenario")) {
			level_.swap(next_scenario);
		}
		else if(revc.has_attribute("version")) {
			level_.swap(revc);
			has_scenario_and_controllers = true;
		}
		else if(config& controllers = revc.child("controllers")) {
			int index = 0;
			for (const config& controller : controllers.child_range("controller")) {
				if(config& side = get_scenario().child("side", index)) {
					side["is_local"] = controller["is_local"];
				}
				++index;
			}
			has_scenario_and_controllers = true;
		}

	}

	DBG_MP << "download_level_data() success.\n";

	return true;
}

config& wait::get_scenario()
{

	if(config& scenario = level_.child("scenario"))
		return scenario;
	else if(config& snapshot = level_.child("snapshot"))
		return snapshot;
	else
		return level_;
}

const config& wait::get_scenario() const
{
	if(const config& scenario = level_.child("scenario"))
		return scenario;
	else if(const config& snapshot = level_.child("snapshot"))
		return snapshot;
	else
		return level_;
}

} // namespace mp

