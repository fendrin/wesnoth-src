/*
   Copyright (C) 2013 by Andrius Silinskas <silinskas.andrius@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include "multiplayer_connect_engine.hpp"

#include "ai/configuration.hpp"
#include "formula_string_utils.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "multiplayer_ui.hpp"
#include "mp_game_utils.hpp"
#include "tod_manager.hpp"

#include <boost/foreach.hpp>

static lg::log_domain log_config("config");
#define LOG_CF LOG_STREAM(info, log_config)
#define WRN_CF LOG_STREAM(warn, log_config)
#define ERR_CF LOG_STREAM(err, log_config)

static lg::log_domain log_mp_connect_engine("mp/connect/engine");
#define DBG_MP LOG_STREAM(debug, log_mp_connect_engine)
#define LOG_MP LOG_STREAM(info, log_mp_connect_engine)

static lg::log_domain log_network("network");
#define LOG_NW LOG_STREAM(info, log_network)

namespace {

const std::string controller_names[] = {
	"network",
	"human",
	"ai",
	"null",
	"reserved"
};

const std::string attributes_to_trim[] = {
	"side",
	"recruit",
	"controller",
	"id",
	"team_name",
	"user_team_name",
	"color",
	"gold",
	"income",
	"allow_changes"
};

}

namespace mp {

connect_engine::connect_engine(game_display& disp, game_state& state,
	const mp_game_settings& params, const bool local_players_only,
	const bool first_scenario) :
	level_(),
	state_(state),
	params_(params),
	default_controller_(local_players_only ? CNTR_LOCAL: CNTR_NETWORK),
	first_scenario_(first_scenario),
	side_configurations_lock_(),
	side_engines_(),
	era_factions_(),
	team_names_(),
	user_team_names_(),
	player_teams_(),
	connected_users_(),
	default_controller_options_()
{
	// Initial level config from the mp_game_settings.
	level_ = initial_level_config(disp, params_, state_);
	if (level_.empty()) {
		return;
	}

	if (params_.saved_game) {
		side_configurations_lock_ = true;
	} else if (!params_.use_map_settings) {
		side_configurations_lock_ = false;
	} else {
		side_configurations_lock_ =
			level()["side_configurations_lock"].to_bool(!first_scenario);
	}

	// Original level sides.
	config::child_itors sides = current_config()->child_range("side");

	// Set the team name lists and modify the original level sides,
	// if necessary.
	std::vector<std::string> original_team_names;
	std::string team_prefix(std::string(_("Team")) + " ");
	int side_count = 1;
	BOOST_FOREACH(config& side, sides) {
		const std::string side_str = lexical_cast<std::string>(side_count);
		config::attribute_value& team_name = side["team_name"];
		config::attribute_value& user_team_name =
			side["user_team_name"];

		// Revert to default values if appropriate.
		if (team_name.empty()) {
			team_name = side_str;
		}
		if (params_.use_map_settings && user_team_name.empty()) {
			user_team_name = team_name;
		}

		bool add_team = true;
		if (params_.use_map_settings) {
			// Only add a team if it is not found.
			bool found = std::find(team_names_.begin(), team_names_.end(),
				team_name.str()) != team_names_.end();

			if (found) {
				add_team = false;
			}
		} else {
			// Always add a new team for every side, but leave
			// the specified team assigned to a side if there is one.
			std::vector<std::string>::const_iterator name_itor =
				std::find(original_team_names.begin(),
					original_team_names.end(), team_name.str());
			if (name_itor == original_team_names.end()) {
				original_team_names.push_back(team_name);

				team_name =
					lexical_cast<std::string>(original_team_names.size());
			} else {
				team_name = lexical_cast<std::string>(
					name_itor - original_team_names.begin() + 1);
			}

			user_team_name = team_prefix + side_str;
		}

		if (add_team) {
			team_names_.push_back(params_.use_map_settings ? team_name :
				side_str);
			user_team_names_.push_back(user_team_name.t_str().to_serialized());

			if (side["allow_player"].to_bool(true)) {
				player_teams_.push_back(user_team_name.str());
			}
		}

		++side_count;
	}

	// Selected era's factions.
	BOOST_FOREACH(const config& era,
		level_.child("era").child_range("multiplayer_side")) {

		era_factions_.push_back(&era);
	}

	// Default options for combo controllers.
	if (!local_players_only) {
		default_controller_options_.push_back(
			std::make_pair(CNTR_NETWORK, _("Network Player")));
	}
	default_controller_options_.push_back(
		std::make_pair(CNTR_LOCAL, _("Local Player")));
	default_controller_options_.push_back(
		std::make_pair(CNTR_COMPUTER, _("Computer Player")));
	default_controller_options_.push_back(
		std::make_pair(CNTR_EMPTY, _("Empty")));

	// Create side engines.
	int index = 0;
	BOOST_FOREACH(const config &s, sides) {
		side_engine_ptr new_side_engine(new side_engine(s, *this, index));
		side_engines_.push_back(new_side_engine);

		index++;
	}

	// Load reserved players information into the sides.
	if (!first_scenario_) {
		std::map<std::string, std::string> side_users =
			utils::map_split(level_.child("multiplayer")["side_users"]);
		BOOST_FOREACH(side_engine_ptr side, side_engines_) {
			const std::string& save_id = side->save_id();
			if (side_users.find(save_id) != side_users.end()) {
				side->set_current_player(side_users[save_id]);

				side->update_controller_options();
				side->set_controller(CNTR_RESERVED);
			}
		}
	}

	// Add host to the connected users list.
	import_user(preferences::login(), false);
}

connect_engine::~connect_engine()
{
}

config* connect_engine::current_config() {
	config* cfg_level = NULL;

	// It might make sense to invent a mechanism of some sort to check
	// whether a config node contains information
	// that you can load from(side information, specifically).
	config &snapshot = level_.child("snapshot");
	if (snapshot && snapshot.child("side")) {
		// Savegame.
		cfg_level = &snapshot;
	} else if (!level_.child("side")) {
		// Start-of-scenario save,
		// the info has to be taken from the starting_pos.
		cfg_level = &state_.replay_start();
	} else {
		// Fresh game, no snapshot available.
		cfg_level = &level_;
	}

	return cfg_level;
}

void connect_engine::import_user(const std::string& name, const bool observer,
	int side_taken)
{
	config user_data;
	user_data["name"] = name;
	import_user(user_data, observer, side_taken);
}

void connect_engine::import_user(const config& data, const bool observer,
	int side_taken)
{
	const std::string& username = data["name"];
	assert(!username.empty());

	connected_users_.insert(username);
	update_side_controller_options();

	if (observer) {
		return;
	}

	bool side_assigned = false;
	if (side_taken >= 0) {
		side_engines_[side_taken]->place_user(data);
		side_assigned = true;
	}

	// Check if user has a side(s) reserved for him.
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		if (side->current_player() == username) {
			side->place_user(data);

			side_assigned = true;
		}
	}

	if (side_taken < 0) {
		// If no sides were assigned for a user,
		// take a first available side.
		if (!side_assigned) {
			BOOST_FOREACH(side_engine_ptr side, side_engines_) {
				if (side->available_for_user(username) ||
					side->controller() == CNTR_LOCAL) {

					side->place_user(data);

					side_assigned = true;
					break;
				}
			}
		}
	}
}

bool connect_engine::sides_available() const
{
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		if (side->available_for_user()) {
			return true;
		}
	}

	return false;
}

void connect_engine::update_level()
{
	DBG_MP << "updating level" << std::endl;

	level_.clear_children("side");

	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		level_.add_child("side", side->new_config());
	}
}

void connect_engine::update_and_send_diff(bool update_time_of_day)
{
	config old_level = level_;
	update_level();

	if (update_time_of_day) {
		// Set random start ToD.
		tod_manager tod_mng(level_, level_["turns"]);
	}

	config diff = level_.get_diff(old_level);
	if (!diff.empty()) {
		config scenario_diff;
		scenario_diff.add_child("scenario_diff", diff);
		network::send_data(scenario_diff, 0);
	}
}

bool connect_engine::can_start_game() const
{
	// First check if all sides are ready to start the game.
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		if (!side->ready_for_start()) {
			const int side_num = side->index() + 1;
			DBG_MP << "not all sides are ready, side " <<
				side_num << " not ready\n";

			return false;
		}
	}

	DBG_MP << "all sides are ready" << std::endl;

	/*
	 * If at least one human player is slotted with a player/ai we're allowed
	 * to start. Before used a more advanced test but it seems people are
	 * creative in what is used in multiplayer [1] so use a simpler test now.
	 * [1] http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=568029
	 */
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		if (side->controller() != CNTR_EMPTY && side->allow_player()) {
			return true;
		}
	}

	return false;
}

void connect_engine::start_game()
{
	DBG_MP << "starting a new game" << std::endl;

    // Resolves the "random faction", "random gender" and "random message"
    // Must be done before shuffle sides, or some cases will cause errors
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		side->resolve_random();
	}

	// Shuffle sides (check preferences and if it is a re-loaded game).
	// Must be done after resolve_random() or shuffle sides, or they won't work.
	if (preferences::shuffle_sides() && !(level_.child("snapshot") &&
		level_.child("snapshot").child("side"))) {

		// Only playable sides should be shuffled.
		std::vector<int> playable_sides;
		BOOST_FOREACH(side_engine_ptr side, side_engines_) {
			if (side->allow_player()) {
				playable_sides.push_back(side->index());
			}
		}

		// Fisher-Yates shuffle.
		for (int i = playable_sides.size(); i > 1; i--)
		{
			int j_side = playable_sides[get_random() % i];
			int i_side = playable_sides[i - 1];

			int tmp_index = side_engines_[j_side]->index();
			side_engines_[j_side]->set_index(side_engines_[i_side]->index());
			side_engines_[i_side]->set_index(tmp_index);

			int tmp_team = side_engines_[j_side]->team();
			side_engines_[j_side]->set_team(side_engines_[i_side]->team());
			side_engines_[i_side]->set_team(tmp_team);

			// This is needed otherwise fog bugs will appear.
			side_engine_ptr tmp_side = side_engines_[j_side];
			side_engines_[j_side] = side_engines_[i_side];
			side_engines_[i_side] = tmp_side;
		}
	}

	// Make other clients not show the results of resolve_random().
	config lock("stop_updates");
	network::send_data(lock, 0);
	update_and_send_diff(true);

	save_reserved_sides_information();

	// Build the gamestate object after updating the level.
	level_to_gamestate(level_, state_);

	network::send_data(config("start_game"), 0);
}

void connect_engine::start_game_commandline(
	const commandline_options& cmdline_opts)
{
	DBG_MP << "starting a new game in commandline mode" << std::endl;

	typedef boost::tuple<unsigned int, std::string> mp_option;

	unsigned num = 0;
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		num++;

		// Set the faction, if commandline option is given.
		if (cmdline_opts.multiplayer_side) {
			BOOST_FOREACH(const mp_option& option,
				*cmdline_opts.multiplayer_side) {

				if (option.get<0>() == num) {
					DBG_MP << "\tsetting side " << option.get<0>() <<
						"\tfaction: " << option.get<1>() << std::endl;

					side->set_faction_commandline(option.get<1>());
				}
			}
		}

		// Set the controller, if commandline option is given.
		if (cmdline_opts.multiplayer_controller) {
			BOOST_FOREACH(const mp_option& option,
				*cmdline_opts.multiplayer_controller) {

				if (option.get<0>() == num) {
					DBG_MP << "\tsetting side " << option.get<0>() <<
						"\tfaction: " << option.get<1>() << std::endl;

					side->set_controller_commandline(option.get<1>());
				}
			}
		}

		// Set AI algorithm to RCA AI for all sides,
		// then override if commandline option was given.
		side->set_ai_algorithm("ai_default_rca");
		if (cmdline_opts.multiplayer_algorithm) {
			BOOST_FOREACH(const mp_option& option,
				*cmdline_opts.multiplayer_algorithm) {

				if (option.get<0>() == num) {
					DBG_MP << "\tsetting side " << option.get<0>() <<
						"\tfaction: " << option.get<1>() << std::endl;

					side->set_ai_algorithm(option.get<1>());
				}
			}
		}

		// Finally, resolve "random faction",
		// "random gender" and "random message", if any remains unresolved.
		side->resolve_random();
	} // end top-level loop

	update_and_send_diff(true);

	// Update sides with commandline parameters.
	if (cmdline_opts.multiplayer_turns) {
		DBG_MP << "\tsetting turns: " << cmdline_opts.multiplayer_turns <<
			std::endl;
		level_["turns"] = *cmdline_opts.multiplayer_turns;
	}

	BOOST_FOREACH(config &side, level_.child_range("side"))
	{
		if (cmdline_opts.multiplayer_ai_config) {
			BOOST_FOREACH(const mp_option& option,
				*cmdline_opts.multiplayer_ai_config) {

				if (option.get<0>() == side["side"].to_unsigned()) {
					DBG_MP << "\tsetting side " << side["side"] <<
						"\tai_config: " << option.get<1>() << std::endl;

					side["ai_config"] = option.get<1>();
				}
			}
		}

		// Having hard-coded values here is undesirable,
		// but that's how it is done in the MP lobby
		// part of the code also.
		// Should be replaced by settings/constants in both places
		if (cmdline_opts.multiplayer_ignore_map_settings) {
			side["gold"] = 100;
			side["income"] = 1;
		}

		typedef boost::tuple<unsigned int, std::string, std::string>
			mp_parameter;

		if (cmdline_opts.multiplayer_parm) {
			BOOST_FOREACH(const mp_parameter& parameter,
				*cmdline_opts.multiplayer_parm) {

				if (parameter.get<0>() == side["side"].to_unsigned()) {
					DBG_MP << "\tsetting side " << side["side"] << " " <<
						parameter.get<1>() << ": " << parameter.get<2>() <<
							std::endl;

					side[parameter.get<1>()] = parameter.get<2>();
				}
			}
		}
    }

	save_reserved_sides_information();

	// Build the gamestate object after updating the level
	level_to_gamestate(level_, state_);
	network::send_data(config("start_game"), 0);
}

std::pair<bool, bool> connect_engine::process_network_data(const config& data,
	const network::connection sock)
{
	std::pair<bool, bool> result(std::make_pair(false, true));

	if (data.child("leave_game")) {
		result.first = true;
		return result;
	}

	// A side has been dropped.
	if (!data["side_drop"].empty()) {
		unsigned side_drop = data["side_drop"].to_int() - 1;

		if (side_drop < side_engines_.size()) {
			side_engine_ptr side_to_drop = side_engines_[side_drop];

			// Remove user, whose side was dropped.
			connected_users_.erase(side_to_drop->player_id());
			update_side_controller_options();

			side_to_drop->reset();

			update_and_send_diff();

			return result;
		}
	}

	// A player is connecting to the game.
	if (!data["side"].empty()) {
		unsigned side_taken = data["side"].to_int() - 1;

		// Checks if the connecting user has a valid and unique name.
		const std::string name = data["name"];
		if (name.empty()) {
			config response;
			response["failed"] = true;
			network::send_data(response, sock);

			ERR_CF << "ERROR: No username provided with the side.\n";

			return result;
		}

		if (connected_users_.find(name) != connected_users_.end()) {
			 // TODO: Seems like a needless limitation
			 // to only allow one side per player.
			if (find_user_side_index_by_id(name) != -1) {
				config response;
				response["failed"] = true;
				response["message"] = "The nickname '" + name +
					"' is already in use.";
				network::send_data(response, sock);

				return result;
			} else {
				connected_users_.erase(name);
				update_side_controller_options();
				config observer_quit;
				observer_quit.add_child("observer_quit")["name"] = name;
				network::send_data(observer_quit, 0);
			}
		}

		// Assigns this user to a side.
		if (side_taken < side_engines_.size()) {
			if (!side_engines_[side_taken]->available_for_user(name)) {
				// This side is already taken.
				// Try to reassing the player to a different position.
				side_taken = 0;
				BOOST_FOREACH(side_engine_ptr s, side_engines_) {
					if (s->available_for_user()) {
						break;
					}

					side_taken++;
				}

				if (side_taken >= side_engines_.size()) {
					config response;
					response["failed"] = true;
					network::send_data(response, sock);

					config res;
					config& kick = res.add_child("kick");
					kick["username"] = data["name"];
					network::send_data(res, 0);

					update_and_send_diff();

					ERR_CF << "ERROR: Couldn't assign a side to '" <<
						name << "'\n";

					return result;
				}
			}

			LOG_CF << "client has taken a valid position\n";

			import_user(data, false, side_taken);

			update_and_send_diff();

			result.second = false;

			LOG_NW << "sent player data\n";
		} else {
			ERR_CF << "tried to take illegal side: " << side_taken << "\n";

			config response;
			response["failed"] = true;
			network::send_data(response, sock);
		}
	}


	if (const config& change_faction = data.child("change_faction")) {
		int side_taken = find_user_side_index_by_id(change_faction["name"]);
		if (side_taken != -1 || !first_scenario_) {
			import_user(change_faction, false, side_taken);

			update_and_send_diff();
		}
	}

	if (const config& observer = data.child("observer")) {
		import_user(observer, true);
		update_and_send_diff();
	}

	if (const config& observer = data.child("observer_quit")) {
		const t_string& observer_name = observer["name"];
		if (!observer_name.empty()) {
			if ((connected_users_.find(observer_name) !=
				connected_users_.end()) &&
				(find_user_side_index_by_id(observer_name) != -1)) {

				connected_users_.erase(observer_name);
				update_side_controller_options();
				update_and_send_diff();
			}
		}
	}

	return result;
}

void connect_engine::process_network_error(network::error& error)
{
	// The problem isn't related to any specific connection and
	// it's a general error. So we should just re-throw the error.
	error.disconnect();
	throw network::error(error.message);
}

void connect_engine::process_network_connection(const network::connection sock)
{
	network::send_data(config("join_game"), 0);
	// If we are connected, send data to the connected host.
	if (first_scenario_) {
		network::send_data(level_, sock);
	} else {
		config next_level;
		next_level.add_child("store_next_scenario", level_);
		network::send_data(next_level, sock);
	}
}

int connect_engine::find_user_side_index_by_id(const std::string& id) const
{
	size_t i = 0;
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		if (side->player_id() == id) {
			break;
		}

		i++;
	}

	if (i >= side_engines_.size()) {
		return -1;
	}

	return i;
}

void connect_engine::save_reserved_sides_information()
{
	// Add information about reserved sides to the level config.
	// N.B. This information is needed only for a host player.
	std::map<std::string, std::string> side_users;
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		const std::string& save_id = side->save_id();
		const std::string& player_id = side->player_id();
		if (!save_id.empty() && !player_id.empty()) {
			side_users[save_id] = player_id;
		}
	}
	level_.child("multiplayer")["side_users"] = utils::join_map(side_users);
}

void connect_engine::update_side_controller_options()
{
	BOOST_FOREACH(side_engine_ptr side, side_engines_) {
		side->update_controller_options();
	}
}

side_engine::side_engine(const config& cfg, connect_engine& parent_engine,
	const int index) :
	cfg_(cfg),
	parent_(parent_engine),
	controller_(CNTR_NETWORK),
	current_controller_index_(0),
	available_factions_(),
	choosable_factions_(),
	choosable_leaders_(),
	choosable_genders_(),
	controller_options_(),
	current_faction_(NULL),
	current_leader_("null"),
	current_gender_("null"),
	allow_player_(cfg["controller"] == "ai" && cfg["allow_player"].empty() ?
		false : cfg["allow_player"].to_bool(true)),
	allow_changes_(cfg["allow_changes"].to_bool(true)),
	leader_id_(cfg["id"]),
	index_(index),
	team_(0),
	color_(index),
	gold_(cfg["gold"].to_int(100)),
	income_(cfg["income"]),
	current_player_(cfg["current_player"]),
	player_id_(cfg["player_id"]),
	ai_algorithm_()
{
	update_controller_options();

	// Tweak the controllers.
	if (cfg_["controller"] == "human_ai" ||
		cfg_["controller"] == "network_ai") {

		cfg_["controller"] = "ai";
	}
	if (!current_player_.empty() && cfg_["controller"] != "ai") {
		// Reserve a side for "current_player", unless the side
		// is played by an AI.
		set_controller(CNTR_RESERVED);
	} else if (allow_player_ && !parent_.params_.saved_game) {
		set_controller(parent_.default_controller_);
	} else {
		size_t i = CNTR_NETWORK;
		if (!allow_player_) {
			if (cfg_["controller"] == "null") {
				set_controller(CNTR_EMPTY);
			} else {
				cfg_["controller"] = "ai";
				set_controller(CNTR_COMPUTER);
			}
		} else {
			if (cfg_["controller"] == "network" ||
				cfg_["controller"] == "human") {

				cfg_["controller"] = "reserved";
			}

			for (; i != CNTR_LAST; ++i) {
				if (cfg_["controller"] == controller_names[i]) {
					set_controller(static_cast<mp::controller>(i));
					break;
				}
			}
		}
	}

	if (parent_.params_.saved_game) {
		// Set faction lock on previous faction.
		cfg_["faction_from_recruit"] = true;
	}

	// Initialize team and color.
	unsigned team_name_index = 0;
	BOOST_FOREACH(const std::string& name, parent_.team_names_) {
		if (name == cfg["team_name"]) {
			break;
		}

		team_name_index++;
	}
	if (team_name_index >= parent_.team_names_.size()) {
		assert(!parent_.team_names_.empty());
		team_ = 0;
	} else {
		team_ = team_name_index;
	}
	if (!cfg["color"].empty()) {
		color_ = game_config::color_info(cfg["color"]).index() - 1;
	}

	// Initialize faction lists.
	available_factions_ = init_available_factions(parent_.era_factions_, cfg_,
		parent_.params_.use_map_settings);

	const bool has_any_recruits =
		!cfg["recruit"].empty() || !cfg["previous_recruits"].empty();
	if ((!cfg_.has_attribute("faction") || !parent_.first_scenario_) &&
		has_any_recruits && parent_.side_configurations_lock_) {

		// By default side should be locked to a first available faction
		// if side configurations lock is on and if side has no
		// predefined faction or it's not a first scenario.
		cfg_["faction"] = (*available_factions_[0])["id"];
	}

	choosable_factions_ = init_choosable_factions(available_factions_, cfg_,
		parent_.params_.use_map_settings);
	assert(!choosable_factions_.empty());

	int faction_index = 0;
	if (parent_.params_.use_map_settings || parent_.params_.saved_game) {
		// Explicitly assign a faction, if possible.
		if (choosable_factions_.size() > 1) {
			faction_index = find_suitable_faction(choosable_factions_, cfg_);
			if (faction_index < 0) {
				faction_index = 0;
			}
		}
	}

	// Initialize ai algorithm.
	if (const config& ai = cfg.child("ai")) {
		ai_algorithm_ = ai["ai_algorithm"].str();
	}

	// Initializes leader and gender lists.
	set_current_faction(choosable_factions_[faction_index]);
}

side_engine::~side_engine()
{
}

config side_engine::new_config() const
{
	config res = cfg_;

	// If the user is allowed to change type, faction, leader etc,
	// then import their new values in the config.
	if (!parent_.params_.saved_game) {
		// Merge the faction data to res.
		res.append(*current_faction_);
		res["faction_name"] = res["name"];
	}

	if (!cfg_.has_attribute("side") || cfg_["side"].to_int() != index_ + 1) {
		res["side"] = index_ + 1;
	}
	res["controller"] = controller_names[controller_];
	res["current_player"] = player_id_.empty() ? current_player_ : player_id_;
	res["id"] = leader_id_;

	if (player_id_.empty()) {
		std::string description;
		switch(controller_) {
		case CNTR_NETWORK:
			description = N_("(Vacant slot)");

			break;
		case CNTR_LOCAL:
			if (!parent_.params_.saved_game && !cfg_.has_attribute("save_id")) {
				res["save_id"] = preferences::login() + res["side"].str();
			}
			res["player_id"] = preferences::login() + res["side"].str();
			res["current_player"] = preferences::login();
			description = N_("Anonymous local player");

			break;
		case CNTR_COMPUTER: {
			if (!parent_.params_.saved_game &&
				!cfg_.has_attribute("saved_id")) {

				res["save_id"] = "ai" + res["side"].str();
			}

			utils::string_map symbols;
			if (allow_player_) {
				const config& ai_cfg =
					ai::configuration::get_ai_config_for(ai_algorithm_);
				res.add_child("ai", ai_cfg);
				symbols["playername"] = ai_cfg["description"];
			} else {
				// Do not import default ai cfg here -
				// all is set by scenario config.
				symbols["playername"] = _("Computer Player");
			}

			symbols["side"] = res["side"].str();
			description = vgettext("$playername $side", symbols);

			break;
		}
		case CNTR_EMPTY:
			description = N_("(Empty slot)");
			res["no_leader"] = true;

			break;
		case CNTR_RESERVED: {
			utils::string_map symbols;
			symbols["playername"] = current_player_;
			description = vgettext("(Reserved for $playername)",symbols);

			break;
		}
		case CNTR_LAST:
		default:
			description = N_("(empty)");
			assert(false);

			break;
		} // end switch

		res["user_description"] = t_string(description, "wesnoth");
	} else {
		res["player_id"] = player_id_ + res["side"];
		if (!parent_.params_.saved_game && !cfg_.has_attribute("save_id")) {
			res["save_id"] = player_id_ + res["side"];
		}

		res["user_description"] = player_id_;
	}

	res["name"] = res["user_description"];
	res["allow_changes"] = !parent_.params_.saved_game && allow_changes_;

	if (!parent_.params_.saved_game) {
		res["type"] = (current_leader_ != "null") ? current_leader_ : "random";
		res["gender"] = (current_gender_ != "null") ? current_gender_ :
			"random";

		res["team_name"] = parent_.team_names_[team_];
		res["user_team_name"] = parent_.user_team_names_[team_];
		res["allow_player"] = allow_player_;
		res["color"] = color_ + 1;
		res["gold"] = gold_;
		res["income"] = income_;

		if (!parent_.params_.use_map_settings || res["fog"].empty() ||
			(res["fog"] != "yes" && res["fog"] != "no")) {
			res["fog"] = parent_.params_.fog_game;
		}

		if (!parent_.params_.use_map_settings || res["shroud"].empty() ||
			(res["shroud"] != "yes" && res["shroud"] != "no")) {
			res["shroud"] = parent_.params_.shroud_game;
		}

		res["share_maps"] = parent_.params_.share_maps;
		res["share_view"] =  parent_.params_.share_view;

		if (!parent_.params_.use_map_settings || res["village_gold"].empty()) {
			res["village_gold"] = parent_.params_.village_gold;
		}
		if (!parent_.params_.use_map_settings ||
			res["village_support"].empty()) {
			res["village_support"] =
				lexical_cast<std::string>(parent_.params_.village_support);
		}

	}

	if (parent_.params_.use_map_settings && !parent_.params_.saved_game) {
		config trimmed = cfg_;

		BOOST_FOREACH(const std::string& attribute, attributes_to_trim) {
			trimmed.remove_attribute(attribute);
		}

		if (controller_ != CNTR_COMPUTER) {
			// Only override names for computer controlled players.
			trimmed.remove_attribute("user_description");
		}

		res.merge_with(trimmed);
	}

	return res;
}

bool side_engine::ready_for_start() const
{
	if (!allow_player_) {
		// Sides without players are always ready.
		return true;
	}

	if ((controller_ == mp::CNTR_COMPUTER) ||
		(controller_ == mp::CNTR_EMPTY) ||
		(controller_ == mp::CNTR_LOCAL)) {

		return true;
	}

	if (controller_ == CNTR_NETWORK && !player_id_.empty()) {
		// Side is assigned to a network player.
		return true;
	}

	return false;
}

bool side_engine::available_for_user(const std::string& name) const
{
	if (!allow_player_) {
		// Computer sides are never available for players.
		return false;
	}

	if (controller_ == CNTR_NETWORK && player_id_.empty()) {
		// Side is free and waiting for user.
		return true;
	}
	if (controller_ == CNTR_RESERVED && name.empty()) {
		// Side is still available to someone.
		return true;
	}
	if (controller_ == CNTR_RESERVED && current_player_ == name) {
		// Side is available only for the player with specific name.
		return true;
	}

	return false;
}

void side_engine::swap_sides_on_drop_target(const int drop_target) {
	const std::string target_id =
		parent_.side_engines_[drop_target]->player_id_;
	const mp::controller target_controller =
		parent_.side_engines_[drop_target]->controller_;
	const std::string target_ai =
		parent_.side_engines_[drop_target]->ai_algorithm_;

	parent_.side_engines_[drop_target]->ai_algorithm_ = ai_algorithm_;
	if (player_id_.empty()) {
		parent_.side_engines_[drop_target]->player_id_.clear();
		parent_.side_engines_[drop_target]->set_controller(controller_);
	} else {
		parent_.side_engines_[drop_target]->place_user(player_id_);
	}

	ai_algorithm_ = target_ai;
	if (target_id.empty()) {
		player_id_.clear();
		set_controller(target_controller);
	} else {
		place_user(target_id);
	}
}

void side_engine::resolve_random()
{
	if (parent_.params_.saved_game) {
		return;
	}

	if ((*current_faction_)["random_faction"].to_bool()) {
		// Choose a random faction, and force leader to be random.
		current_leader_ = "random";

		std::vector<std::string> faction_choices, faction_excepts;

		faction_choices = utils::split((*current_faction_)["choices"]);
		if (faction_choices.size() == 1 && faction_choices.front() == "") {
			faction_choices.clear();
		}

		faction_excepts = utils::split((*current_faction_)["except"]);
		if (faction_excepts.size() == 1 && faction_excepts.front() == "") {
			faction_excepts.clear();
		}

		// Builds the list of factions eligible for choice
		// (non-random factions).
		std::vector<int> nonrandom_sides;
		int num = -1;
		BOOST_FOREACH(const config* i, available_factions_) {
			++num;
			if (!(*i)["random_faction"].to_bool()) {
				const std::string& faction_id = (*i)["id"];

				if (!faction_choices.empty() &&
					std::find(faction_choices.begin(), faction_choices.end(),
						faction_id) == faction_choices.end()) {
					continue;
				}

				if (!faction_excepts.empty() &&
					std::find(faction_excepts.begin(), faction_excepts.end(),
						faction_id) != faction_excepts.end()) {
					continue;
				}

				nonrandom_sides.push_back(num);
			}
		}

		if (nonrandom_sides.empty()) {
			throw config::error(_("Only random sides in the current era."));
		}

		const int faction_index =
			nonrandom_sides[rand() % nonrandom_sides.size()];
		set_current_faction(available_factions_[faction_index]);
	}

	LOG_MP << "FACTION" << (index_ + 1) << ": " << (*current_faction_)["name"]
		<< std::endl;

	bool solved_random_leader = false;

	if (current_leader_ == "random") {
		// Choose a random leader type, and force gender to be random.
		current_gender_ = "random";

		std::vector<std::string> nonrandom_leaders;
		BOOST_FOREACH(const std::string& leader, choosable_leaders_) {
			if (leader != "random") {
				nonrandom_leaders.push_back(leader);
			}
		}

		if (nonrandom_leaders.empty()) {
			utils::string_map i18n_symbols;
			i18n_symbols["faction"] = (*current_faction_)["name"];
			throw config::error(vgettext(
				"Unable to find a leader type for faction $faction",
				i18n_symbols));
		} else {
			const int lchoice = rand() % nonrandom_leaders.size();
			set_current_leader(nonrandom_leaders[lchoice]);
		}

		solved_random_leader = true;
	}

	// Resolve random genders "very much" like standard unit code.
	if (current_gender_ == "random" || solved_random_leader) {
		const unit_type *ut = unit_types.find(current_leader_);
		if (ut && !choosable_genders_.empty()) {
			std::vector<std::string> nonrandom_genders;
			BOOST_FOREACH(const std::string& gender, choosable_genders_) {
				if (gender != "random") {
					nonrandom_genders.push_back(gender);
				}
			}

			const int gchoice = rand() % nonrandom_genders.size();
			set_current_gender(nonrandom_genders[gchoice]);
		} else {
			ERR_CF << "cannot obtain genders for invalid leader '" <<
				current_leader_ << "'.\n";
			current_gender_ = "null";
		}
	}
}

void side_engine::reset()
{
	player_id_.clear();
	set_controller(parent_.default_controller_);

	if (!parent_.params_.saved_game) {
		set_current_faction(choosable_factions_[0]);
		set_current_leader(choosable_leaders_[0]);
		set_current_gender(choosable_genders_[0]);
	}
}

void side_engine::place_user(const std::string& name)
{
	config data;
	data["name"] = name;

	place_user(data);
}

void side_engine::place_user(const config& data)
{
	player_id_ = data["name"].str();
	set_controller(parent_.default_controller_);

	if (data.has_attribute("faction")) {
		// Network user's data carry information about chosen
		// faction, leader and genders.
		BOOST_FOREACH(const config* faction, choosable_factions_) {
			if ((*faction)["id"] == data["faction"]) {
				set_current_faction(faction);
			}
		}
		set_current_leader(data["leader"]);
		set_current_gender(data["gender"]);
	}
}

void side_engine::update_controller_options()
{
	controller_options_ = parent_.default_controller_options_;
	if (!current_player_.empty()) {
		controller_options_.push_back(
			std::make_pair(CNTR_RESERVED, _("Reserved")));
	}

	controller_options_.push_back(std::make_pair(CNTR_LAST, _("--give--")));

	BOOST_FOREACH(const std::string& user, parent_.connected_users_) {
		controller_options_.push_back(std::make_pair(
			parent_.default_controller_, user));
	}

	update_current_controller_index();
}

void side_engine::update_current_controller_index()
{
	int i = 0;
	BOOST_FOREACH(const controller_option& option, controller_options_) {
		if (option.first == controller_) {
			current_controller_index_ = i;

			if (player_id_.empty() || player_id_ == option.second) {
				// Stop searching if no user is assigned to a side
				// or the selected user is found.
				break;
			}
		}

		i++;
	}

	assert(current_controller_index_ < controller_options_.size());
}

bool side_engine::controller_changed(const int selection)
{
	const mp::controller selected_cntr = controller_options_[selection].first;
	if (selected_cntr == CNTR_LAST) {
		return false;
	}

	// Check if user was selected. If so assign a side to him/her.
	// If not, make sure that no user is assigned to this side.
	if (selected_cntr == parent_.default_controller_ && selection != 0) {
		player_id_ = controller_options_[selection].second;
	} else {
		player_id_.clear();
	}

	set_controller(selected_cntr);

	return true;
}

void side_engine::set_controller(mp::controller controller)
{
	controller_ = controller;

	update_current_controller_index();
}

void side_engine::set_current_faction(const config* current_faction)
{
	current_faction_ = current_faction;

	update_choosable_leaders();
	set_current_leader(choosable_leaders_[0]);
}

void side_engine::set_current_leader(const std::string& current_leader)
{
	current_leader_ = current_leader;

	update_choosable_genders();
	set_current_gender(choosable_genders_[0]);
}

void side_engine::set_current_gender(const std::string& current_gender)
{
	current_gender_ = current_gender;
}

int side_engine::current_faction_index() const
{
	int index = 0;
	BOOST_FOREACH(const config* faction, choosable_factions_) {
		if ((*faction)["id"] == (*current_faction_)["id"]) {
			return index;
		}

		index++;
	}

	return 0;
}

void side_engine::set_faction_commandline(const std::string& faction_name)
{
	BOOST_FOREACH(const config* faction, choosable_factions_) {
		if ((*faction)["name"] == faction_name) {
			current_faction_ = faction;
			break;
		}
	}
}

void side_engine::set_controller_commandline(const std::string& controller_name)
{
	set_controller(CNTR_LOCAL);

	if (controller_name == "ai") {
		set_controller(CNTR_COMPUTER);
	}
	if (controller_name == "null") {
		set_controller(CNTR_EMPTY);
	}

	player_id_.clear();
}

void side_engine::update_choosable_leaders()
{
	choosable_leaders_.clear();
	choosable_leaders_ = init_choosable_leaders(cfg_, current_faction_,
		available_factions_, parent_.params_.use_map_settings,
		parent_.params_.saved_game);
}

void side_engine::update_choosable_genders()
{
	choosable_genders_.clear();
	choosable_genders_ = init_choosable_genders(cfg_, current_leader_,
		parent_.params_.use_map_settings, parent_.params_.saved_game);
}

} // end namespace mp
