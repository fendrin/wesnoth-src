/* $Id$ */
/*
   Copyright (C) 2003 - 2012 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Recruiting, Fighting.
 */

#include "actions.hpp"

#include "attack_prediction.hpp"
#include "foreach.hpp"
#include "game_display.hpp"
#include "game_events.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "hotkeys.hpp"
#include "map_label.hpp"
#include "mouse_handler_base.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "statistics.hpp"
#include "unit_abilities.hpp"
#include "unit_display.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "tod_manager.hpp"
#include "whiteboard/manager.hpp"

static lg::log_domain log_engine("engine");
#define DBG_NG LOG_STREAM(debug, log_engine)
#define LOG_NG LOG_STREAM(info, log_engine)
#define ERR_NG LOG_STREAM(err, log_engine)

static lg::log_domain log_config("config");
#define LOG_CF LOG_STREAM(info, log_config)

static lg::log_domain log_ai_testing("ai/testing");
#define LOG_AI_TESTING LOG_STREAM(info, log_ai_testing)

struct castle_cost_calculator : pathfind::cost_calculator
{
	castle_cost_calculator(const gamemap& map) : map_(map)
	{}

	virtual double cost(const map_location& loc, const double) const
	{
		if(!map_.is_castle(loc))
			return 10000;

		return 1;
	}

private:
	const gamemap& map_;
};

void move_unit_spectator::add_seen_friend(const unit_map::const_iterator &u)
{
	seen_friends_.push_back(u);
}


void move_unit_spectator::add_seen_enemy(const unit_map::const_iterator &u)
{
	seen_enemies_.push_back(u);
}


const unit_map::const_iterator& move_unit_spectator::get_ambusher() const
{
	return ambusher_;
}


const unit_map::const_iterator& move_unit_spectator::get_failed_teleport() const
{
	return failed_teleport_;
}


const std::vector<unit_map::const_iterator>& move_unit_spectator::get_seen_enemies() const
{
	return seen_enemies_;
}


const std::vector<unit_map::const_iterator>& move_unit_spectator::get_seen_friends() const
{
	return seen_friends_;
}


const unit_map::const_iterator& move_unit_spectator::get_unit() const
{
	return unit_;
}


move_unit_spectator::move_unit_spectator(const unit_map &units)
	: ambusher_(units.end()),failed_teleport_(units.end()),seen_enemies_(),seen_friends_(),unit_(units.end())
{
}


move_unit_spectator::~move_unit_spectator()
{
}

void move_unit_spectator::reset(const unit_map &units)
{
	ambusher_ = units.end();
	failed_teleport_ = units.end();
	seen_enemies_.clear();
	seen_friends_.clear();
	unit_ = units.end();
}


void move_unit_spectator::set_ambusher(const unit_map::const_iterator &u)
{
	ambusher_ = u;
}


void move_unit_spectator::set_failed_teleport(const unit_map::const_iterator &u)
{
	failed_teleport_ = u;
}


void move_unit_spectator::set_unit(const unit_map::const_iterator &u)
{
	unit_ = u;
}


unit_creator::unit_creator(team &tm, const map_location &start_pos)
  : add_to_recall_(false),discover_(false),get_village_(false),invalidate_(false), rename_side_(false), show_(false), start_pos_(start_pos), team_(tm)
{
}


unit_creator& unit_creator::allow_show(bool b)
{
	show_=b;
	return *this;
}


unit_creator& unit_creator::allow_get_village(bool b)
{
	get_village_=b;
	return *this;
}


unit_creator& unit_creator::allow_rename_side(bool b)
{
	rename_side_=b;
	return *this;
}

unit_creator& unit_creator::allow_invalidate(bool b)
{
	invalidate_=b;
	return *this;
}


unit_creator& unit_creator::allow_discover(bool b)
{
	discover_=b;
	return *this;
}


unit_creator& unit_creator::allow_add_to_recall(bool b)
{
	add_to_recall_=b;
	return *this;
}


map_location unit_creator::find_location(const config &cfg, const unit* pass_check)
{

	DBG_NG << "finding location for unit with id=["<<cfg["id"]<<"] placement=["<<cfg["placement"]<<"] x=["<<cfg["x"]<<"] y=["<<cfg["y"]<<"] for side " << team_.side() << "\n";

	std::vector< std::string > placements = utils::split(cfg["placement"]);

	placements.push_back("map");
	placements.push_back("recall");

	foreach(std::string place, placements) {
		map_location loc;
		bool pass((place == "leader_passable") || (place == "map_passable"));

		if (place == "recall" ) {
			return map_location::null_location;
		}

		if (place == "leader" || place == "leader_passable") {
			unit_map::const_iterator leader = resources::units->find_leader(team_.side());
			//todo: take 'leader in recall list' possibility into account
			if (leader.valid()) {
				loc = leader->get_location();
			} else {
				loc = start_pos_;
			}
		}

		if (place=="map" || place == "map_passable") {
			loc = map_location(cfg,resources::state_of_game);
		}

		if(loc.valid() && resources::game_map->on_board(loc)) {
			if (pass) {
		  		loc = find_vacant_tile(*resources::game_map, *resources::units, loc, pathfind::VACANT_ANY, pass_check);
			} else {
				loc = find_vacant_tile(*resources::game_map, *resources::units, loc, pathfind::VACANT_ANY);
			}
			if(loc.valid() && resources::game_map->on_board(loc)) {
				return loc;
			}
		}
	}

	return map_location::null_location;

}


void unit_creator::add_unit(const config &cfg, const vconfig* vcfg)
{
	config temp_cfg(cfg);
	temp_cfg["side"] = team_.side();
	temp_cfg.remove_attribute("player_id");
	temp_cfg.remove_attribute("faction_from_recruit");

	const std::string& id =(cfg)["id"];
	bool animate = temp_cfg["animate"].to_bool();
	temp_cfg.remove_attribute("animate");

	std::vector<unit> &recall_list = team_.recall_list();
	std::vector<unit>::iterator recall_list_element = find_if_matches_id(recall_list, id);

	if ( recall_list_element == recall_list.end() ) {
		//make a temporary unit
		boost::scoped_ptr<unit> temp_unit(new unit(temp_cfg, true, resources::state_of_game, vcfg));
		map_location loc = find_location(temp_cfg, temp_unit.get());
		if(!loc.valid()) {
			if(add_to_recall_) {
				//add to recall list
				unit *new_unit = temp_unit.get();
				recall_list.push_back(*new_unit);
				DBG_NG << "inserting unit with id=["<<id<<"] on recall list for side " << new_unit->side() << "\n";
				preferences::encountered_units().insert(new_unit->type_id());
			}
		} else {
			unit *new_unit = temp_unit.get();
			//add temporary unit to map
			assert( resources::units->find(loc) == resources::units->end() );
			resources::units->add(loc, *new_unit);
			LOG_NG << "inserting unit for side " << new_unit->side() << "\n";
			post_create(loc,*(resources::units->find(loc)),animate);
		}
	} else {
		//get unit from recall list
		map_location loc = find_location(temp_cfg, &(*recall_list_element));
		if(!loc.valid()) {
			LOG_NG << "wanted to insert unit on recall list, but recall list for side " << (cfg)["side"] << "already contains id=" <<id<<"\n";
			return;
		} else {
			resources::units->add(loc, *recall_list_element);
			LOG_NG << "inserting unit from recall list for side " << recall_list_element->side()<< " with id="<< id << "\n";
			post_create(loc,*(resources::units->find(loc)),animate);
			//if id is not empty, delete units with this ID from recall list
			erase_if_matches_id(recall_list, id);
		}
	}
}


void unit_creator::post_create(const map_location &loc, const unit &new_unit, bool anim)
{

	if (discover_) {
		preferences::encountered_units().insert(new_unit.type_id());
	}

	bool show = show_ && (resources::screen !=NULL) && !resources::screen->fogged(loc);
	bool animate = show && anim;

	if (get_village_) {
		if (resources::game_map->is_village(loc)) {
			get_village(loc, new_unit.side());
		}
	}

	if (resources::screen!=NULL) {

		if (invalidate_ ) {
			resources::screen->invalidate(loc);
		}

		if (animate) {
			unit_display::unit_recruited(loc);
		} else if (show) {
			resources::screen->draw();
		}
	}
}


bool can_recruit_on(const gamemap& map, const map_location& leader, const map_location& loc)
{
	if(!map.on_board(loc))
		return false;

	if(!map.is_castle(loc))
		return false;

	if(!map.is_keep(leader))
		return false;

	castle_cost_calculator calc(map);
	// The limit computed in the third argument is more than enough for
	// any convex castle on the map. Strictly speaking it could be
	// reduced to sqrt(map.w()**2 + map.h()**2).
	pathfind::plain_route rt = pathfind::a_star_search(leader, loc, map.w()+map.h(), &calc, map.w(), map.h());
	return !rt.steps.empty();
}

const std::set<std::string> get_recruits_for_location(int side, const map_location &recruit_loc)
{
	LOG_NG << "getting recruit list for side " << side << " at location " << recruit_loc << "\n";

	const std::set<std::string>& recruit_list = (*resources::teams)[side -1].recruits();
	std::set<std::string> local_result;
	std::set<std::string> global_result;

	unit_map::const_iterator u = resources::units->begin(),
			u_end = resources::units->end();

	bool leader_in_place = false;
	bool recruit_loc_is_castle = resources::game_map->is_castle(recruit_loc);

	for(; u != u_end; ++u) {
		//Only consider leaders on this side.
		if (!(u->can_recruit() && u->side() == side))
			continue;

		//Check if the leader is on the right keep.
		if (resources::game_map->is_keep(u->get_location())
				&& (can_recruit_on(*resources::game_map, u->get_location(), recruit_loc))) {
			leader_in_place= true;
			local_result.insert(u->recruits().begin(), u->recruits().end());
		} else
			global_result.insert(u->recruits().begin(), u->recruits().end());
	}

	bool global = !(recruit_loc_is_castle && leader_in_place);

	if (global)
		global_result.insert(recruit_list.begin(),recruit_list.end());
	else if (leader_in_place)
		local_result.insert(recruit_list.begin(),recruit_list.end());

	return global ? global_result : local_result;
}

const std::vector<const unit*> get_recalls_for_location(int side, const map_location &recall_loc) {
	LOG_NG << "getting recall list for side " << side << " at location " << recall_loc << "\n";

	const team& t = (*resources::teams)[side-1];
	const std::vector<unit>& recall_list = t.recall_list();
	std::vector<const unit*> result;

	/*
	 * We have two use cases:
	 * 1. A castle tile is highlighted, we only present the units recallable there.
	 * 2. A non castle tile is highlighted, we present all units in the recall list.
	 */

	bool leader_in_place = false;
	bool recall_loc_is_castle = resources::game_map->is_castle(recall_loc);

	if (recall_loc_is_castle) {

		unit_map::const_iterator u = resources::units->begin(),
				u_end = resources::units->end();
		std::set<size_t> valid_local_recalls;

		for(; u != u_end; ++u) {
			//We only consider leaders on our side.
			if (!(u->can_recruit() && u->side() == side))
				continue;

			//Check if the leader is on the right keep.
			if (resources::game_map->is_keep(u->get_location())
					&& (can_recruit_on(*resources::game_map, u->get_location(), recall_loc)))
				leader_in_place= true;
			else continue;

			foreach (const unit& recall_unit, recall_list)
			{
				//Only units which match the leaders recall filter are valid.
				scoped_recall_unit this_unit("this_unit", t.save_id(), &recall_unit - &recall_list[0]);
				if (!(recall_unit.matches_filter(vconfig(u->recall_filter()), map_location::null_location)))
					continue;

				//Do not add a unit twice.
				if (valid_local_recalls.find(recall_unit.underlying_id())
						== valid_local_recalls.end()) {
				valid_local_recalls.insert(recall_unit.underlying_id());
				result.push_back(&recall_unit);
				}
			}
		}
	}

	if (!(recall_loc_is_castle && leader_in_place)) {
		foreach (const unit &recall, recall_list)
		{
			result.push_back(&recall);
		}
	}

	return result;
}

std::string find_recall_location(const int side, map_location& recall_loc, map_location& recall_from, const unit &recall_unit)
{
	LOG_NG << "finding recall location for side " << side << " and unit " << recall_unit.id() << "\n";

	unit_map::const_iterator u = resources::units->begin(),
		u_end = resources::units->end(), leader = u_end, leader_keep = u_end, leader_fit = u_end,
			leader_able = u_end, leader_opt = u_end;

	map_location alternate_location = map_location::null_location;
	map_location alternate_from = map_location::null_location;

	for(; u != u_end; ++u) {
		//quit if it is not a leader on the @side
		if (!(u->can_recruit() && u->side() == side))
			continue;
		leader = u;

		//quit if the leader is not able to recall the @recall_unit
		const team& t = (*resources::teams)[side-1];
		scoped_recall_unit this_unit("this_unit",
			t.save_id(),
			&recall_unit - &t.recall_list()[0]);
		if (!(recall_unit.matches_filter(vconfig(leader->recall_filter()), map_location::null_location)))
			continue;
		leader_able = leader;

		//quit if the leader is not on a keep
		if (!(resources::game_map->is_keep(leader->get_location())))
			continue;
		leader_keep = leader_able;

		//find a place to recall in the leader's keep
		map_location tmp_location = find_vacant_tile(*resources::game_map, *resources::units, leader_keep->get_location(),
				pathfind::VACANT_CASTLE);

		//quit if their is no place to recruit on
		if (tmp_location == map_location::null_location)
			continue;
		leader_fit = leader_keep;

		if (can_recruit_on(*resources::game_map, leader_keep->get_location(), recall_loc)) {
			leader_opt = leader_fit;
			if (resources::units->count(recall_loc) == 1)
				recall_loc = tmp_location;
				recall_from = leader_opt->get_location();
			break;
		} else {
			alternate_location = tmp_location;
			alternate_from = leader_fit->get_location();
		}
	}

	if (leader == u_end) {
		LOG_NG << "No Leader on side " << side << " when recalling " << recall_unit.id() << '\n';
		return _("You don’t have a leader to recall with.");
	}

	if (leader_able == u_end) {
		LOG_NG << "No Leader able to recall unit: " << recall_unit.id() << " on side " << side << '\n';
		return _("None of your leaders is able to recall that unit.");
	}

	if (leader_keep == u_end) {
		LOG_NG << "No Leader on a keep to recall the unit " << recall_unit.id() << " at " << recall_loc  << '\n';
		return _("You must have a leader on a keep who is able to recall that unit.");
	}

	if (leader_fit == u_end) {
		LOG_NG << "No vacant castle tiles on a keep available to recall the unit " << recall_unit.id() << " at " << recall_loc  << '\n';
		return _("There are no vacant castle tiles in which to recall the unit.");
	}

	if (leader_opt == u_end) {
		recall_loc = alternate_location;
		recall_from = alternate_from;
	}

	return std::string();
}

std::string find_recruit_location(const int side, map_location& recruit_location, map_location& recruited_from, const std::string& unit_type)
{
	LOG_NG << "finding recruit location for side " << side << "\n";

	unit_map::const_iterator u = resources::units->begin(), u_end = resources::units->end(),
			leader = u_end, leader_keep = u_end, leader_fit = u_end,
			leader_able = u_end, leader_opt = u_end;

	map_location alternate_location = map_location::null_location;
	map_location alternate_from = map_location::null_location;

	const std::set<std::string>& recruit_list = (*resources::teams)[side -1].recruits();
	std::set<std::string>::const_iterator recruit_it = recruit_list.find(unit_type);
	bool is_on_team_list = (recruit_it != recruit_list.end());

	for(; u != u_end; ++u) {
		if (!(u->can_recruit() && u->side() == side))
			continue;
		leader = u;

		bool can_recruit_unit = is_on_team_list;
		if (!can_recruit_unit) {
			foreach (const std::string &recruitable, leader->recruits()) {
				if (recruitable == unit_type) {
					can_recruit_unit = true;
					break;
				}
			}
		}

		if (!can_recruit_unit)
			continue;
		leader_able = leader;

		if (!(resources::game_map->is_keep(leader_able->get_location())))
			continue;
		leader_keep = leader_able;

		map_location tmp_location = find_vacant_tile(*resources::game_map, *resources::units, leader_keep->get_location(),
				pathfind::VACANT_CASTLE);

		if (tmp_location == map_location::null_location)
			continue;
		leader_fit = leader_keep;

		if (can_recruit_on(*resources::game_map, leader_fit->get_location(), recruit_location)) {
			leader_opt = leader_fit;
			if (resources::units->count(recruit_location) == 1)
				recruit_location = tmp_location;
				recruited_from = leader_opt->get_location();
			break;
		} else {
			alternate_location = tmp_location;
			alternate_from = leader_fit->get_location();
		}
	}

	if (leader == u_end) {
		LOG_NG << "No Leader on side " << side << " when recruiting " << unit_type << '\n';
		return _("You don’t have a leader to recruit with.");
	}

	if (leader_keep == u_end) {
		LOG_NG << "Leader not on start: leader is on " << leader->get_location() << '\n';
		return _("You must have a leader on a keep who is able to recruit the unit.");
	}

	if (leader_fit == u_end) {
		LOG_NG << "No vacant castle tiles on a keep available to recruit the unit " << unit_type << " at " << recruit_location  << '\n';
		return _("There are no vacant castle tiles in which to recruit the unit.");
	}

	if (leader_opt == u_end) {
		recruit_location = alternate_location;
		recruited_from = alternate_from;
	}

	return std::string();
}

void place_recruit(const unit &u, const map_location &recruit_location, const map_location& recruited_from,
    bool is_recall, bool show, bool fire_event, bool full_movement,
    bool wml_triggered)
{
	LOG_NG << "placing new unit on location " << recruit_location << "\n";

	assert(resources::units->count(recruit_location) == 0);

	unit new_unit = u;
	if (full_movement) {
		new_unit.set_movement(new_unit.total_movement());
	} else {
		new_unit.set_movement(0);
		new_unit.set_attacks(0);
	}
	new_unit.heal_all();
	new_unit.set_hidden(true);

	resources::units->add(recruit_location, new_unit);

	if (is_recall)
	{
		if (fire_event) {
			LOG_NG << "firing prerecall event\n";
			game_events::fire("prerecall",recruit_location, recruited_from);
		}
	}
	else
	{
		LOG_NG << "firing prerecruit event\n";
		game_events::fire("prerecruit",recruit_location, recruited_from);
	}
	const unit_map::iterator new_unit_itor = resources::units->find(recruit_location);
	if (new_unit_itor.valid()) {
		new_unit_itor->set_hidden(false);
		if (resources::game_map->is_village(recruit_location)) {
			get_village(recruit_location,new_unit_itor->side());
		}
		preferences::encountered_units().insert(new_unit_itor->type_id());

	}

	unit_map::iterator leader = resources::units->begin();
	for(; leader != resources::units->end(); ++leader)
		if (leader->can_recruit() &&
		    leader->side() == new_unit.side() &&
		    resources::game_map->is_keep(leader->get_location()) &&
		    can_recruit_on(*resources::game_map, leader->get_location(), recruit_location))
			break;
	if (show) {
		if (leader.valid()) {
			unit_display::unit_recruited(recruit_location, leader->get_location());
		} else {
			unit_display::unit_recruited(recruit_location);
		}
	}
	if (is_recall)
	{
		if (fire_event) {
			LOG_NG << "firing recall event\n";
			game_events::fire("recall", recruit_location, recruited_from);
		}
	}
	else
	{
		LOG_NG << "firing recruit event\n";
		game_events::fire("recruit",recruit_location, recruited_from);
	}

	const std::string checksum = get_checksum(new_unit);

	const config* ran_results = get_random_results();
	if(ran_results != NULL) {
		// When recalling from WML there should be no random results, if we use
		// random we might get the replay out of sync.
		assert(!wml_triggered);
		const std::string rc = (*ran_results)["checksum"];
		if(rc != checksum) {
			std::stringstream error_msg;
			error_msg << "SYNC: In recruit " << new_unit.type_id() <<
				": has checksum " << checksum <<
				" while datasource has checksum " <<
				rc << "\n";
			ERR_NG << error_msg.str();

			config cfg_unit1;
			new_unit.write(cfg_unit1);
			DBG_NG << cfg_unit1;
			replay::process_error(error_msg.str());
		}

	} else if(wml_triggered == false) {
		config cfg;
		cfg["checksum"] = checksum;
		set_random_results(cfg);
	}

	resources::whiteboard->on_gamestate_change();
}

map_location under_leadership(const unit_map& units,
		const map_location& loc, int* bonus)
{

	const unit_map::const_iterator un = units.find(loc);
	if(un == units.end()) {
		return map_location::null_location;
	}
	unit_ability_list abil = un->get_abilities("leadership");
	if(bonus) {
		*bonus = abil.highest("value").first;
	}
	return abil.highest("value").second;
}

battle_context::battle_context(const unit_map& units,
		const map_location& attacker_loc, const map_location& defender_loc,
		int attacker_weapon, int defender_weapon, double aggression, const combatant *prev_def, const unit* attacker_ptr)
: attacker_stats_(NULL), defender_stats_(NULL), attacker_combatant_(NULL), defender_combatant_(NULL)
{
	const unit &attacker = attacker_ptr ? *attacker_ptr : *units.find(attacker_loc);
	const unit &defender = *units.find(defender_loc);
	const double harm_weight = 1.0 - aggression;

	if (attacker_weapon == -1 && attacker.attacks().size() == 1 && attacker.attacks()[0].attack_weight() > 0 )
		attacker_weapon = 0;

	if (attacker_weapon == -1) {
		attacker_weapon = choose_attacker_weapon(attacker, defender, units,
			attacker_loc, defender_loc,
				harm_weight, &defender_weapon, prev_def);
	} else if (defender_weapon == -1) {
		defender_weapon = choose_defender_weapon(attacker, defender, attacker_weapon,
			units, attacker_loc, defender_loc, prev_def);
	}

	// If those didn't have to generate statistics, do so now.
	if (!attacker_stats_) {
		const attack_type *adef = NULL;
		const attack_type *ddef = NULL;
		if (attacker_weapon >= 0) {
			VALIDATE(attacker_weapon < static_cast<int>(attacker.attacks().size()),
					_("An invalid attacker weapon got selected."));
			adef = &attacker.attacks()[attacker_weapon];
		}
		if (defender_weapon >= 0) {
			VALIDATE(defender_weapon < static_cast<int>(defender.attacks().size()),
					_("An invalid defender weapon got selected."));
			ddef = &defender.attacks()[defender_weapon];
		}
		assert(!defender_stats_ && !attacker_combatant_ && !defender_combatant_);
		attacker_stats_ = new battle_context_unit_stats(attacker, attacker_loc, attacker_weapon,
				true, defender, defender_loc, ddef, units);
		defender_stats_ = new battle_context_unit_stats(defender, defender_loc, defender_weapon, false,
				attacker, attacker_loc, adef, units);
	}
}

	battle_context::battle_context(const battle_context &other)
: attacker_stats_(NULL), defender_stats_(NULL), attacker_combatant_(NULL), defender_combatant_(NULL)
{
	*this = other;
}

battle_context::battle_context(const battle_context_unit_stats &att, const battle_context_unit_stats &def) :
	attacker_stats_(new battle_context_unit_stats(att)),
	defender_stats_(new battle_context_unit_stats(def)),
	attacker_combatant_(0),
	defender_combatant_(0)
{
}

battle_context::~battle_context()
{
	delete attacker_stats_;
	delete defender_stats_;
	delete attacker_combatant_;
	delete defender_combatant_;
}
battle_context& battle_context::operator=(const battle_context &other)
{
	if (&other != this) {
		delete attacker_stats_;
		delete defender_stats_;
		delete attacker_combatant_;
		delete defender_combatant_;
		attacker_stats_ = new battle_context_unit_stats(*other.attacker_stats_);
		defender_stats_ = new battle_context_unit_stats(*other.defender_stats_);
		attacker_combatant_ = other.attacker_combatant_ ? new combatant(*other.attacker_combatant_, *attacker_stats_) : NULL;
		defender_combatant_ = other.defender_combatant_ ? new combatant(*other.defender_combatant_, *defender_stats_) : NULL;
	}
	return *this;
}

/** @todo FIXME: Hand previous defender unit in here. */
int battle_context::choose_defender_weapon(const unit &attacker, const unit &defender, unsigned attacker_weapon,
	const unit_map& units,
		const map_location& attacker_loc, const map_location& defender_loc,
		const combatant *prev_def)
{
	VALIDATE(attacker_weapon < attacker.attacks().size(),
			_("An invalid attacker weapon got selected."));
	const attack_type &att = attacker.attacks()[attacker_weapon];
	std::vector<unsigned int> choices;

	// What options does defender have?
	unsigned int i;
	for (i = 0; i < defender.attacks().size(); ++i) {
		const attack_type &def = defender.attacks()[i];
		if (def.range() == att.range() && def.defense_weight() > 0) {
			choices.push_back(i);
		}
	}
	if (choices.empty())
		return -1;
	if (choices.size() == 1)
		return choices[0];

	// Multiple options:
	// First pass : get the best weight and the minimum simple rating for this weight.
	// simple rating = number of blows * damage per blows (resistance taken in account) * cth * weight
	// Elligible attacks for defense should have a simple rating greater or equal to this weight.

	double max_weight = 0.0;
	int min_rating = 0;

	for (i = 0; i < choices.size(); ++i) {
		const attack_type &def = defender.attacks()[choices[i]];
		if (def.defense_weight() >= max_weight) {
			max_weight = def.defense_weight();
			const battle_context_unit_stats def_stats(defender, defender_loc,
					choices[i], false, attacker, attacker_loc, &att, units);
			int rating = static_cast<int>(def_stats.num_blows * def_stats.damage *
					def_stats.chance_to_hit * def.defense_weight());
			if (def.defense_weight() > max_weight || rating < min_rating ) {
				min_rating = rating;
			}
		}
	}

	// Multiple options: simulate them, save best.
	for (i = 0; i < choices.size(); ++i) {
		const attack_type &def = defender.attacks()[choices[i]];
		battle_context_unit_stats *att_stats = new battle_context_unit_stats(attacker, attacker_loc, attacker_weapon,
				true, defender, defender_loc, &def, units);
		battle_context_unit_stats *def_stats = new battle_context_unit_stats(defender, defender_loc, choices[i], false,
				attacker, attacker_loc, &att, units);

		combatant *att_comb = new combatant(*att_stats);
		combatant *def_comb = new combatant(*def_stats, prev_def);
		att_comb->fight(*def_comb);

		int simple_rating = static_cast<int>(def_stats->num_blows *
				def_stats->damage * def_stats->chance_to_hit * def.defense_weight());

		if (simple_rating >= min_rating &&
				( !attacker_combatant_ || better_combat(*def_comb, *att_comb, *defender_combatant_, *attacker_combatant_, 1.0) )
		   ) {
			delete attacker_combatant_;
			delete defender_combatant_;
			delete attacker_stats_;
			delete defender_stats_;
			attacker_combatant_ = att_comb;
			defender_combatant_ = def_comb;
			attacker_stats_ = att_stats;
			defender_stats_ = def_stats;
		} else {
			delete att_comb;
			delete def_comb;
			delete att_stats;
			delete def_stats;
		}
	}

	return defender_stats_->attack_num;
}

int battle_context::choose_attacker_weapon(const unit &attacker, const unit &defender,
	const unit_map& units,
		const map_location& attacker_loc, const map_location& defender_loc,
		double harm_weight, int *defender_weapon, const combatant *prev_def)
{
	std::vector<unsigned int> choices;

	// What options does attacker have?
	unsigned int i;
	for (i = 0; i < attacker.attacks().size(); ++i) {
		const attack_type &att = attacker.attacks()[i];
		if (att.attack_weight() > 0) {
			choices.push_back(i);
		}
	}
	if (choices.empty())
		return -1;
	if (choices.size() == 1) {
		*defender_weapon = choose_defender_weapon(attacker, defender, choices[0], units,
			attacker_loc, defender_loc, prev_def);
		return choices[0];
	}

	// Multiple options: simulate them, save best.
	battle_context_unit_stats *best_att_stats = NULL, *best_def_stats = NULL;
	combatant *best_att_comb = NULL, *best_def_comb = NULL;

	for (i = 0; i < choices.size(); ++i) {
		const attack_type &att = attacker.attacks()[choices[i]];
		int def_weapon = choose_defender_weapon(attacker, defender, choices[i], units,
			attacker_loc, defender_loc, prev_def);
		// If that didn't simulate, do so now.
		if (!attacker_combatant_) {
			const attack_type *def = NULL;
			if (def_weapon >= 0) {
				def = &defender.attacks()[def_weapon];
			}
			attacker_stats_ = new battle_context_unit_stats(attacker, attacker_loc, choices[i],
				true, defender, defender_loc, def, units);
			defender_stats_ = new battle_context_unit_stats(defender, defender_loc, def_weapon, false,
				attacker, attacker_loc, &att, units);
			attacker_combatant_ = new combatant(*attacker_stats_);
			defender_combatant_ = new combatant(*defender_stats_, prev_def);
			attacker_combatant_->fight(*defender_combatant_);
		}
		if (!best_att_comb || better_combat(*attacker_combatant_, *defender_combatant_,
					*best_att_comb, *best_def_comb, harm_weight)) {
			delete best_att_comb;
			delete best_def_comb;
			delete best_att_stats;
			delete best_def_stats;
			best_att_comb = attacker_combatant_;
			best_def_comb = defender_combatant_;
			best_att_stats = attacker_stats_;
			best_def_stats = defender_stats_;
		} else {
			delete attacker_combatant_;
			delete defender_combatant_;
			delete attacker_stats_;
			delete defender_stats_;
		}
		attacker_combatant_ = NULL;
		defender_combatant_ = NULL;
		attacker_stats_ = NULL;
		defender_stats_ = NULL;
	}

	attacker_combatant_ = best_att_comb;
	defender_combatant_ = best_def_comb;
	attacker_stats_ = best_att_stats;
	defender_stats_ = best_def_stats;

	*defender_weapon = defender_stats_->attack_num;
	return attacker_stats_->attack_num;
}

// Does combat A give us a better result than combat B?
bool battle_context::better_combat(const combatant &us_a, const combatant &them_a,
		const combatant &us_b, const combatant &them_b,
		double harm_weight)
{
	double a, b;

	// Compare: P(we kill them) - P(they kill us).
	a = them_a.hp_dist[0] - us_a.hp_dist[0] * harm_weight;
	b = them_b.hp_dist[0] - us_b.hp_dist[0] * harm_weight;
	if (a - b < -0.01)
		return false;
	if (a - b > 0.01)
		return true;

	// Add poison to calculations
	double poison_a_us = (us_a.poisoned) * game_config::poison_amount;
	double poison_a_them = (them_a.poisoned) * game_config::poison_amount;
	double poison_b_us = (us_b.poisoned) * game_config::poison_amount;
	double poison_b_them = (them_b.poisoned) * game_config::poison_amount;
	// Compare: damage to them - damage to us (average_hp replaces -damage)
	a = (us_a.average_hp()-poison_a_us)*harm_weight - (them_a.average_hp()-poison_a_them);
	b = (us_b.average_hp()-poison_b_us)*harm_weight - (them_b.average_hp()-poison_b_them);
	if (a - b < -0.01)
		return false;
	if (a - b > 0.01)
		return true;

	// All else equal: go for most damage.
	return them_a.average_hp() < them_b.average_hp();
}

/** @todo FIXME: better to initialize combatant initially (move into battle_context_unit_stats?), just do fight() when required. */
const combatant &battle_context::get_attacker_combatant(const combatant *prev_def)
{
	// We calculate this lazily, since AI doesn't always need it.
	if (!attacker_combatant_) {
		assert(!defender_combatant_);
		attacker_combatant_ = new combatant(*attacker_stats_);
		defender_combatant_ = new combatant(*defender_stats_, prev_def);
		attacker_combatant_->fight(*defender_combatant_);
	}
	return *attacker_combatant_;
}

const combatant &battle_context::get_defender_combatant(const combatant *prev_def)
{
	// We calculate this lazily, since AI doesn't always need it.
	if (!defender_combatant_) {
		assert(!attacker_combatant_);
		attacker_combatant_ = new combatant(*attacker_stats_);
		defender_combatant_ = new combatant(*defender_stats_, prev_def);
		attacker_combatant_->fight(*defender_combatant_);
	}
	return *defender_combatant_;
}

battle_context_unit_stats::battle_context_unit_stats(const unit &u, const map_location& u_loc,
		int u_attack_num, bool attacking,
		const unit &opp, const map_location& opp_loc,
		const attack_type *opp_weapon,
		const unit_map& units) :
	weapon(0),
	attack_num(u_attack_num),
	is_attacker(attacking),
	is_poisoned(u.get_state(unit::STATE_POISONED)),
	is_slowed(u.get_state(unit::STATE_SLOWED)),
	slows(false),
	drains(false),
	petrifies(false),
	plagues(false),
	poisons(false),
	backstab_pos(false),
	swarm(false),
	firststrike(false),
	experience(u.experience()),
	max_experience(u.max_experience()),
	level(u.level()),
	rounds(1),
	hp(0),
	max_hp(u.max_hitpoints()),
	chance_to_hit(0),
	damage(0),
	slow_damage(0),
	drain_percent(0),
	drain_constant(0),
	num_blows(0),
	swarm_min(0),
	swarm_max(0),
	plague_type()
{
	// Get the current state of the unit.
	if (attack_num >= 0) {
		weapon = &u.attacks()[attack_num];
	}
	if(u.hitpoints() < 0) {
		LOG_CF << "Unit with " << u.hitpoints() << " hitpoints found, set to 0 for damage calculations\n";
		hp = 0;
	} else if(u.hitpoints() > u.max_hitpoints()) {
		// If a unit has more hp as it's maximum the engine will fail
		// with an assertion failure due to accessing the prob_matrix
		// out of bounds.
		hp = u.max_hitpoints();
	} else {
		hp = u.hitpoints();
	}
	const map_location* aloc = &u_loc;
	const map_location* dloc = &opp_loc;

	if (!attacking)
	{
		aloc = &opp_loc;
		dloc = &u_loc;
	}

	// Get the weapon characteristics, if any.
	if (weapon) {
		weapon->set_specials_context(*aloc, *dloc, units, attacking, opp_weapon);
		if (opp_weapon)
			opp_weapon->set_specials_context(*aloc, *dloc, units, !attacking, weapon);
		bool not_living = opp.get_state("not_living");
		slows = weapon->get_special_bool("slow");
		drains = !not_living && weapon->get_special_bool("drains");
		petrifies = weapon->get_special_bool("petrifies");
		poisons = !not_living && weapon->get_special_bool("poison") && !opp.get_state(unit::STATE_POISONED);
		backstab_pos = is_attacker && backstab_check(u_loc, opp_loc, units, *resources::teams);
		rounds = weapon->get_specials("berserk").highest("value", 1).first;
		firststrike = weapon->get_special_bool("firststrike");

		// Handle plague.
		unit_ability_list plague_specials = weapon->get_specials("plague");
		plagues = !not_living && !plague_specials.empty() &&
			strcmp(opp.undead_variation().c_str(), "null") && !resources::game_map->is_village(opp_loc);

		if (plagues) {
			plague_type = (*plague_specials.cfgs.front().first)["type"].str();
			if (plague_type.empty())
				plague_type = u.type_id();
		}

		// Compute chance to hit.
		chance_to_hit = opp.defense_modifier(resources::game_map->get_terrain(opp_loc)) + weapon->accuracy() - (opp_weapon ? opp_weapon->parry() : 0);
		if(chance_to_hit > 100) {
			chance_to_hit = 100;
		}

		unit_ability_list cth_specials = weapon->get_specials("chance_to_hit");
		unit_abilities::effect cth_effects(cth_specials, chance_to_hit, backstab_pos);
		chance_to_hit = cth_effects.get_composite_value();

		// Compute base damage done with the weapon.
		int base_damage = weapon->damage();
		unit_ability_list dmg_specials = weapon->get_specials("damage");
		unit_abilities::effect dmg_effect(dmg_specials, base_damage, backstab_pos);
		base_damage = dmg_effect.get_composite_value();

		// Get the damage multiplier applied to the base damage of the weapon.
		int damage_multiplier = 100;

		// Time of day bonus.
		damage_multiplier += combat_modifier(u_loc, u.alignment(), u.is_fearless());

		// Leadership bonus.
		int leader_bonus = 0;
		if (under_leadership(units, u_loc, &leader_bonus).valid())
			damage_multiplier += leader_bonus;

		// Resistance modifier.
		damage_multiplier *= opp.damage_from(*weapon, !attacking, opp_loc);

		// Compute both the normal and slowed damage.
		damage = round_damage(base_damage, damage_multiplier, 10000);
		slow_damage = round_damage(base_damage, damage_multiplier, 20000);
		if (is_slowed)
			damage = slow_damage;

		// Compute drain amounts only if draining is possible.
		if(drains) {
			unit_ability_list drain_specials = weapon->get_specials("drains");

			// Compute the drain percent (with 50% as the base for backward compatibility)
			unit_abilities::effect drain_percent_effects(drain_specials, 50, backstab_pos);
			drain_percent = drain_percent_effects.get_composite_value();
		}

		// Add heal_on_hit (the drain constant)
		unit_ability_list heal_on_hit_specials = weapon->get_specials("heal_on_hit");
		unit_abilities::effect heal_on_hit_effects(heal_on_hit_specials, 0, backstab_pos);
		drain_constant += heal_on_hit_effects.get_composite_value();

		drains = drain_constant || drain_percent;

		// Compute the number of blows and handle swarm.
		unit_ability_list swarm_specials = weapon->get_specials("swarm");

		if (!swarm_specials.empty()) {
			swarm = true;
			swarm_min = swarm_specials.highest("swarm_attacks_min").first;
			swarm_max = swarm_specials.highest("swarm_attacks_max", weapon->num_attacks()).first;
			num_blows = swarm_min + (swarm_max - swarm_min) * hp / max_hp;
		} else {
			swarm = false;
			num_blows = weapon->num_attacks();
			unit_ability_list attacks_specials = weapon->get_specials("attacks");
			unit_abilities::effect attacks_effect(attacks_specials,num_blows,backstab_pos);
			const int num_blows_with_specials = attacks_effect.get_composite_value();
			if(num_blows_with_specials >= 0) {
				num_blows = num_blows_with_specials;
			}
			else { ERR_NG << "negative number of strikes after applying weapon specials\n"; }
			swarm_min = num_blows;
			swarm_max = num_blows;
		}
	}
}

battle_context_unit_stats::~battle_context_unit_stats()
{
}

void battle_context_unit_stats::dump() const
{
	printf("==================================\n");
	printf("is_attacker:	%d\n", static_cast<int>(is_attacker));
	printf("is_poisoned:	%d\n", static_cast<int>(is_poisoned));
	printf("is_slowed:	%d\n", static_cast<int>(is_slowed));
	printf("slows:		%d\n", static_cast<int>(slows));
	printf("drains:		%d\n", static_cast<int>(drains));
	printf("petrifies:	%d\n", static_cast<int>(petrifies));
	printf("poisons:	%d\n", static_cast<int>(poisons));
	printf("backstab_pos:	%d\n", static_cast<int>(backstab_pos));
	printf("swarm:		%d\n", static_cast<int>(swarm));
	printf("rounds:	%d\n", static_cast<int>(rounds));
	printf("firststrike:	%d\n", static_cast<int>(firststrike));
	printf("\n");
	printf("hp:		%d\n", hp);
	printf("max_hp:		%d\n", max_hp);
	printf("chance_to_hit:	%d\n", chance_to_hit);
	printf("damage:		%d\n", damage);
	printf("slow_damage:	%d\n", slow_damage);
	printf("drain_percent:	%d\n", drain_percent);
	printf("drain_constant:	%d\n", drain_constant);
	printf("num_blows:	%d\n", num_blows);
	printf("swarm_min:	%d\n", swarm_min);
	printf("swarm_max:	%d\n", swarm_max);
	printf("\n");
}

// Given this harm_weight, are we better than this other context?
bool battle_context::better_attack(class battle_context &that, double harm_weight)
{
	return better_combat(get_attacker_combatant(), get_defender_combatant(),
			that.get_attacker_combatant(), that.get_defender_combatant(), harm_weight);
}

/** Helper class for performing an attack. */
class attack
{
	friend void attack_unit(const map_location &, const map_location &,
		int, int, bool);

	attack(const map_location &attacker, const map_location &defender,
		int attack_with, int defend_with, bool update_display = true);
	void perform();
	bool perform_hit(bool, statistics::attack_context &);
	~attack();

	class attack_end_exception {};
	void fire_event(const std::string& n);
	void refresh_bc();

	/** Structure holding unit info used in the attack action. */
	struct unit_info
	{
		const map_location loc_;
		int weapon_;
		unit_map &units_;
		size_t id_; /**< unit.underlying_id() */
		std::string weap_id_;
		int orig_attacks_;
		int n_attacks_; /**< Number of attacks left. */
		int cth_;
		int damage_;
		int xp_;

		unit_info(const map_location &loc, int weapon, unit_map &units);
		unit &get_unit();
		bool valid();

		std::string dump();
	};

	void unit_killed(unit_info &, unit_info &,
		const battle_context_unit_stats *&, const battle_context_unit_stats *&,
		bool);

	battle_context *bc_;
	const battle_context_unit_stats *a_stats_;
	const battle_context_unit_stats *d_stats_;

	int abs_n_attack_, abs_n_defend_;
	// update_att_fog_ is not used, other than making some code simpler.
	bool update_att_fog_, update_def_fog_, update_minimap_;

	unit_info a_, d_;
	unit_map &units_;
	std::ostringstream errbuf_;

	bool update_display_;
	bool OOS_error_;
};

void attack::fire_event(const std::string& n)
{
	LOG_NG << "firing " << n << " event\n";
	//prepare the event data for weapon filtering
	config ev_data;
	config& a_weapon_cfg = ev_data.add_child("first");
	config& d_weapon_cfg = ev_data.add_child("second");
	if(a_stats_->weapon != NULL && a_.valid()) {
		a_weapon_cfg = a_stats_->weapon->get_cfg();
	}
	if(d_stats_->weapon != NULL && d_.valid()) {
		d_weapon_cfg = d_stats_->weapon->get_cfg();
	}
	if(a_weapon_cfg["name"].empty()) {
		a_weapon_cfg["name"] = "none";
	}
	if(d_weapon_cfg["name"].empty()) {
		d_weapon_cfg["name"] = "none";
	}
	if(n == "attack_end") {
		// We want to fire attack_end event in any case! Even if one of units was removed by WML
		game_events::fire(n, a_.loc_, d_.loc_, ev_data);
		return;
	}
	const int defender_side = d_.get_unit().side();
	game_events::fire(n, game_events::entity_location(a_.loc_, a_.id_),
		game_events::entity_location(d_.loc_, d_.id_), ev_data);

	// The event could have killed either the attacker or
	// defender, so we have to make sure they still exist
	refresh_bc();
	if(!a_.valid() || !d_.valid() || !(*resources::teams)[a_.get_unit().side() - 1].is_enemy(d_.get_unit().side())) {
		if (update_display_){
			recalculate_fog(defender_side);
			resources::screen->recalculate_minimap();
			resources::screen->draw(true, true);
		}
		fire_event("attack_end");
		throw attack_end_exception();
	}
}

namespace {
	void refresh_weapon_index(int& weap_index, std::string const& weap_id, std::vector<attack_type> const& attacks) {
		if(attacks.empty()) {
			//no attacks to choose from
			weap_index = -1;
			return;
		}
		if(weap_index >= 0 && weap_index < static_cast<int>(attacks.size()) && attacks[weap_index].id() == weap_id) {
			//the currently selected attack fits
			return;
		}
		if(!weap_id.empty()) {
			//lookup the weapon by id
			for(int i=0; i<static_cast<int>(attacks.size()); ++i) {
				if(attacks[i].id() == weap_id) {
					weap_index = i;
					return;
				}
			}
		}
		//lookup has failed
		weap_index = -1;
		return;
	}
} //end anonymous namespace

void attack::refresh_bc()
{
	// Fix index of weapons
	if (a_.valid()) {
		refresh_weapon_index(a_.weapon_, a_.weap_id_, a_.get_unit().attacks());
	}
	if (d_.valid()) {
		refresh_weapon_index(d_.weapon_, d_.weap_id_, d_.get_unit().attacks());
	}
	if(!a_.valid() || !d_.valid()) {
		// Fix pointer to weapons
		const_cast<battle_context_unit_stats*>(a_stats_)->weapon =
			a_.valid() && a_.weapon_ >= 0
				? &a_.get_unit().attacks()[a_.weapon_] : NULL;

		const_cast<battle_context_unit_stats*>(d_stats_)->weapon =
			d_.valid() && d_.weapon_ >= 0
				? &d_.get_unit().attacks()[d_.weapon_] : NULL;

		return;
	}

	*bc_ =	battle_context(units_, a_.loc_, d_.loc_, a_.weapon_, d_.weapon_);
	a_stats_ = &bc_->get_attacker_stats();
	d_stats_ = &bc_->get_defender_stats();
	a_.cth_ = a_stats_->chance_to_hit;
	d_.cth_ = d_stats_->chance_to_hit;
	a_.damage_ = a_stats_->damage;
	d_.damage_ = d_stats_->damage;
}

attack::~attack()
{
	delete bc_;
}

attack::unit_info::unit_info(const map_location& loc, int weapon, unit_map& units) :
	loc_(loc),
	weapon_(weapon),
	units_(units),
	id_(),
	weap_id_(),
	orig_attacks_(0),
	n_attacks_(0),
	cth_(0),
	damage_(0),
	xp_(0)
{
	unit_map::iterator i = units_.find(loc_);
	if (!i.valid()) return;
	id_ = i->underlying_id();
}

unit &attack::unit_info::get_unit()
{
	unit_map::iterator i = units_.find(loc_);
	assert(i.valid() && i->underlying_id() == id_);
	return *i;
}

bool attack::unit_info::valid()
{
	unit_map::iterator i = units_.find(loc_);
	return i.valid() && i->underlying_id() == id_;
}

std::string attack::unit_info::dump()
{
	std::stringstream s;
	s << get_unit().type_id() << " (" << loc_.x + 1 << ',' << loc_.y + 1 << ')';
	return s.str();
}

void attack_unit(const map_location &attacker, const map_location &defender,
	int attack_with, int defend_with, bool update_display)
{
	attack dummy(attacker, defender, attack_with, defend_with, update_display);
	dummy.perform();
}

attack::attack(const map_location &attacker, const map_location &defender,
		int attack_with,
		int defend_with,
		bool update_display) :
	bc_(0),
	a_stats_(0),
	d_stats_(0),
	abs_n_attack_(0),
	abs_n_defend_(0),
	update_att_fog_(false),
	update_def_fog_(false),
	update_minimap_(false),
	a_(attacker, attack_with, *resources::units),
	d_(defender, defend_with, *resources::units),
	units_(*resources::units),
	errbuf_(),
	update_display_(update_display),
	OOS_error_(false)
{
}

bool attack::perform_hit(bool attacker_turn, statistics::attack_context &stats)
{
	unit_info
		&attacker = *(attacker_turn ? &a_ : &d_),
		&defender = *(attacker_turn ? &d_ : &a_);
	const battle_context_unit_stats
		*&attacker_stats = *(attacker_turn ? &a_stats_ : &d_stats_),
		*&defender_stats = *(attacker_turn ? &d_stats_ : &a_stats_);
	int &abs_n = *(attacker_turn ? &abs_n_attack_ : &abs_n_defend_);
	bool &update_fog = *(attacker_turn ? &update_def_fog_ : &update_att_fog_);

	int ran_num = get_random();
	bool hits = (ran_num % 100) < attacker.cth_;

	int damage = 0;
	if (hits) {
		damage = attacker.damage_;
		resources::state_of_game->get_variable("damage_inflicted") = damage;
	}

	// Make sure that if we're serializing a game here,
	// we got the same results as the game did originally.
	const config *ran_results = get_random_results();
	if (ran_results)
	{
		int results_chance = (*ran_results)["chance"];
		bool results_hits = (*ran_results)["hits"].to_bool();
		int results_damage = (*ran_results)["damage"];

		if (results_chance != attacker.cth_)
		{
			errbuf_ << "SYNC: In attack " << a_.dump() << " vs " << d_.dump()
				<< ": chance to hit is inconsistent. Data source: "
				<< results_chance << "; Calculation: " << attacker.cth_
				<< " (over-riding game calculations with data source results)\n";
			attacker.cth_ = results_chance;
			OOS_error_ = true;
		}

		if (results_hits != hits)
		{
			errbuf_ << "SYNC: In attack " << a_.dump() << " vs " << d_.dump()
				<< ": the data source says the hit was "
				<< (results_hits ? "successful" : "unsuccessful")
				<< ", while in-game calculations say the hit was "
				<< (hits ? "successful" : "unsuccessful")
				<< " random number: " << ran_num << " = "
				<< (ran_num % 100) << "/" << results_chance
				<< " (over-riding game calculations with data source results)\n";
			hits = results_hits;
			OOS_error_ = true;
		}

		if (results_damage != damage)
		{
			errbuf_ << "SYNC: In attack " << a_.dump() << " vs " << d_.dump()
				<< ": the data source says the hit did " << results_damage
				<< " damage, while in-game calculations show the hit doing "
				<< damage
				<< " damage (over-riding game calculations with data source results)\n";
			damage = results_damage;
			OOS_error_ = true;
		}
	}

	int damage_done = std::min<int>(defender.get_unit().hitpoints(), attacker.damage_);

	int drains_damage = 0;
	if (hits && attacker_stats->drains) {
		drains_damage = damage_done * attacker_stats->drain_percent / 100 + attacker_stats->drain_constant;
		// don't drain so much that the attacker gets more than his maximum hitpoints
		drains_damage = std::min<int>(drains_damage, attacker.get_unit().max_hitpoints() - attacker.get_unit().hitpoints());
		// if drain is negative, don't allow drain to kill the attacker
		drains_damage = std::max<int>(drains_damage, 1 - attacker.get_unit().hitpoints());
	}

	if (update_display_)
	{
		std::ostringstream float_text;
		if (hits)
		{
			const unit &defender_unit = defender.get_unit();
			if (attacker_stats->poisons && !defender_unit.get_state(unit::STATE_POISONED)) {
				float_text << (defender_unit.gender() == unit_race::FEMALE ?
					_("female^poisoned") : _("poisoned")) << '\n';
			}

			if (attacker_stats->slows && !defender_unit.get_state(unit::STATE_SLOWED)) {
				float_text << (defender_unit.gender() == unit_race::FEMALE ?
					_("female^slowed") : _("slowed")) << '\n';
			}

			if (attacker_stats->petrifies) {
				float_text << (defender_unit.gender() == unit_race::FEMALE ?
					_("female^petrified") : _("petrified")) << '\n';
			}
		}

		unit_display::unit_attack(attacker.loc_, defender.loc_, damage,
			*attacker_stats->weapon, defender_stats->weapon,
			abs_n, float_text.str(), drains_damage, "");
	}

	bool dies = defender.get_unit().take_hit(damage);
	LOG_NG << "defender took " << damage << (dies ? " and died\n" : "\n");
	if (attacker_turn) {
		stats.attack_result(hits
			? (dies ? statistics::attack_context::KILLS : statistics::attack_context::HITS)
			: statistics::attack_context::MISSES, damage_done, drains_damage);
	} else {
		stats.defend_result(hits
			? (dies ? statistics::attack_context::KILLS : statistics::attack_context::HITS)
			: statistics::attack_context::MISSES, damage_done, drains_damage);
	}

	if (!ran_results)
	{
		log_scope2(log_engine, "setting random results");
		config cfg;
		cfg["hits"] = hits;
		cfg["dies"] = dies;
		cfg["unit_hit"] = "defender";
		cfg["damage"] = damage;
		cfg["chance"] = attacker.cth_;

		set_random_results(cfg);
	}
	else
	{
		bool results_dies = (*ran_results)["dies"].to_bool();
		if (results_dies != dies)
		{
			errbuf_ << "SYNC: In attack " << a_.dump() << " vs " << d_.dump()
				<< ": the data source says the "
				<< (attacker_turn ? "defender" : "attacker") << ' '
				<< (results_dies ? "perished" : "survived")
				<< " while in-game calculations show it "
				<< (dies ? "perished" : "survived")
				<< " (over-riding game calculations with data source results)\n";
			dies = results_dies;
			// Set hitpoints to 0 so later checks don't invalidate the death.
			// Maybe set to > 0 for the else case to avoid more errors?
			if (results_dies) defender.get_unit().set_hitpoints(0);
			OOS_error_ = true;
		}
	}

	if (hits)
	{
		try {
			fire_event(attacker_turn ? "attacker_hits" : "defender_hits");
		} catch (attack_end_exception) {
			refresh_bc();
			return false;
		}
	}
	else
	{
		try {
			fire_event(attacker_turn ? "attacker_misses" : "defender_misses");
		} catch (attack_end_exception) {
			refresh_bc();
			return false;
		}
	}
	refresh_bc();

	bool attacker_dies = false;
	if (drains_damage > 0) {
		attacker.get_unit().heal(drains_damage);
	} else if(drains_damage < 0) {
		attacker_dies = attacker.get_unit().take_hit(-drains_damage);
	}

	if (dies) {
		unit_killed(attacker, defender, attacker_stats, defender_stats, false);
		update_fog = true;
	}
	if (attacker_dies) {
		unit_killed(defender, attacker, defender_stats, attacker_stats, true);
		*(attacker_turn ? &update_att_fog_ : &update_def_fog_) = true;
	}

	if(dies) {
		update_minimap_ = true;
		return false;
	}

	if (hits)
	{
		unit &defender_unit = defender.get_unit();
		if (attacker_stats->poisons && !defender_unit.get_state(unit::STATE_POISONED)) {
			defender_unit.set_state(unit::STATE_POISONED, true);
			LOG_NG << "defender poisoned\n";
		}

		if (attacker_stats->slows && !defender_unit.get_state(unit::STATE_SLOWED)) {
			defender_unit.set_state(unit::STATE_SLOWED, true);
			update_fog = true;
			defender.damage_ = defender_stats->slow_damage;
			LOG_NG << "defender slowed\n";
		}

		// If the defender is petrified, the fight stops immediately
		if (attacker_stats->petrifies) {
			defender_unit.set_state(unit::STATE_PETRIFIED, true);
			update_fog = true;
			attacker.n_attacks_ = 0;
			defender.n_attacks_ = -1; // Petrified.
			game_events::fire("petrified", defender.loc_, attacker.loc_);
			refresh_bc();
		}
	}

	// Delay until here so that poison and slow go through
	if(attacker_dies) {
		update_minimap_ = true;
		return false;
	}

	--attacker.n_attacks_;
	return true;
}

void attack::unit_killed(unit_info& attacker, unit_info& defender,
	const battle_context_unit_stats *&attacker_stats, const battle_context_unit_stats *&defender_stats,
	bool drain_killed)
{
	attacker.xp_ = game_config::kill_xp(defender.get_unit().level());
	defender.xp_ = 0;
	resources::screen->invalidate(attacker.loc_);

	game_events::entity_location death_loc(defender.loc_, defender.id_);
	game_events::entity_location attacker_loc(attacker.loc_, attacker.id_);
	std::string undead_variation = defender.get_unit().undead_variation();
	fire_event("attack_end");
	refresh_bc();

	// get weapon info for last_breath and die events
	config dat;
	config a_weapon_cfg = attacker_stats->weapon && attacker.valid() ?
		attacker_stats->weapon->get_cfg() : config();
	config d_weapon_cfg = defender_stats->weapon && defender.valid() ?
		defender_stats->weapon->get_cfg() : config();
	if (a_weapon_cfg["name"].empty())
		a_weapon_cfg["name"] = "none";
	if (d_weapon_cfg["name"].empty())
		d_weapon_cfg["name"] = "none";
	dat.add_child("first",  d_weapon_cfg);
	dat.add_child("second", a_weapon_cfg);

	game_events::fire("last breath", death_loc, attacker_loc, dat);
	refresh_bc();

	if (!defender.valid() || defender.get_unit().hitpoints() > 0) {
		// WML has invalidated the dying unit, abort
		return;
	}

	if (!attacker.valid()) {
		unit_display::unit_die(defender.loc_, defender.get_unit(),
			NULL, defender_stats->weapon);
	} else {
		unit_display::unit_die(defender.loc_, defender.get_unit(),
			attacker_stats->weapon, defender_stats->weapon,
			attacker.loc_, &attacker.get_unit());
	}

	game_events::fire("die", death_loc, attacker_loc, dat);
	refresh_bc();

	if (!defender.valid() || defender.get_unit().hitpoints() > 0) {
		// WML has invalidated the dying unit, abort
		return;
	}

	units_.erase(defender.loc_);

	if (attacker.valid() && attacker_stats->plagues && !drain_killed)
	{
		// plague units make new units on the target hex
		LOG_NG << "trying to reanimate " << attacker_stats->plague_type << '\n';
		const unit_type *reanimator =
			unit_types.find(attacker_stats->plague_type);
		if (reanimator)
		{
			LOG_NG << "found unit type:" << reanimator->id() << '\n';
			unit newunit(reanimator, attacker.get_unit().side(),
				true, unit_race::MALE);
			newunit.set_attacks(0);
			newunit.set_movement(0);
			// Apply variation
			if (undead_variation != "null")
			{
				config mod;
				config &variation = mod.add_child("effect");
				variation["apply_to"] = "variation";
				variation["name"] = undead_variation;
				newunit.add_modification("variation",mod);
				newunit.heal_all();
			}
			units_.add(death_loc, newunit);
			preferences::encountered_units().insert(newunit.type_id());
			if (update_display_) {
				resources::screen->invalidate(death_loc);
			}
		}
	}
	else
	{
		LOG_NG << "unit not reanimated\n";
	}
}

void attack::perform()
{
	// Stop the user from issuing any commands while the units are fighting
	const events::command_disabler disable_commands;

	if(!a_.valid() || !d_.valid()) {
		return;
	}

	// no attack weapon => stop here and don't attack
	if (a_.weapon_ < 0) {
		a_.get_unit().set_attacks(a_.get_unit().attacks_left()-1);
		a_.get_unit().set_movement(-1);
		return;
	}

	a_.get_unit().set_attacks(a_.get_unit().attacks_left()-1);
	VALIDATE(a_.weapon_ < static_cast<int>(a_.get_unit().attacks().size()),
		_("An invalid attacker weapon got selected."));
	a_.get_unit().set_movement(a_.get_unit().movement_left() -
		a_.get_unit().attacks()[a_.weapon_].movement_used());
	a_.get_unit().set_state(unit::STATE_NOT_MOVED,false);
	a_.get_unit().set_resting(false);
	d_.get_unit().set_resting(false);

	// If the attacker was invisible, she isn't anymore!
	a_.get_unit().set_state(unit::STATE_UNCOVERED, true);

	bc_ = new battle_context(units_, a_.loc_, d_.loc_, a_.weapon_, d_.weapon_);
	a_stats_ = &bc_->get_attacker_stats();
	d_stats_ = &bc_->get_defender_stats();
	if(a_stats_->weapon) {
		a_.weap_id_ = a_stats_->weapon->id();
	}
	if(d_stats_->weapon) {
		d_.weap_id_ = d_stats_->weapon->id();
	}

	try {
		fire_event("attack");
	} catch (attack_end_exception) {
		return;
	}
	refresh_bc();

	DBG_NG << "getting attack statistics\n";
	statistics::attack_context attack_stats(a_.get_unit(), d_.get_unit(), a_stats_->chance_to_hit, d_stats_->chance_to_hit);

	{
		// Calculate stats for battle
		combatant attacker(bc_->get_attacker_stats());
		combatant defender(bc_->get_defender_stats());
		attacker.fight(defender,false);
		const double attacker_inflict = static_cast<double>(d_.get_unit().hitpoints()) - defender.average_hp();
		const double defender_inflict = static_cast<double>(a_.get_unit().hitpoints()) - attacker.average_hp();

		attack_stats.attack_expected_damage(attacker_inflict,defender_inflict);
	}

	a_.orig_attacks_ = a_stats_->num_blows;
	d_.orig_attacks_ = d_stats_->num_blows;
	a_.n_attacks_ = a_.orig_attacks_;
	d_.n_attacks_ = d_.orig_attacks_;
	a_.xp_ = d_.get_unit().level();
	d_.xp_ = a_.get_unit().level();

	bool defender_strikes_first = (d_stats_->firststrike && !a_stats_->firststrike);
	unsigned int rounds = std::max<unsigned int>(a_stats_->rounds, d_stats_->rounds) - 1;
	const int defender_side = d_.get_unit().side();

	LOG_NG << "Fight: (" << a_.loc_ << ") vs (" << d_.loc_ << ") ATT: " << a_stats_->weapon->name() << " " << a_stats_->damage << "-" << a_stats_->num_blows << "(" << a_stats_->chance_to_hit << "%) vs DEF: " << (d_stats_->weapon ? d_stats_->weapon->name() : "none") << " " << d_stats_->damage << "-" << d_stats_->num_blows << "(" << d_stats_->chance_to_hit << "%)" << (defender_strikes_first ? " defender first-strike" : "") << "\n";

	// Play the pre-fight animation
	unit_display::unit_draw_weapon(a_.loc_,a_.get_unit(),a_stats_->weapon,d_stats_->weapon,d_.loc_,&d_.get_unit());

	for (;;)
	{
		DBG_NG << "start of attack loop...\n";
		++abs_n_attack_;

		if (a_.n_attacks_ > 0 && !defender_strikes_first) {
			if (!perform_hit(true, attack_stats)) break;
		}

		// If the defender got to strike first, they use it up here.
		defender_strikes_first = false;
		++abs_n_defend_;

		if (d_.n_attacks_ > 0) {
			if (!perform_hit(false, attack_stats)) break;
		}

		// Continue the fight to death; if one of the units got petrified,
		// either n_attacks or n_defends is -1
		if(rounds > 0 && d_.n_attacks_ == 0 && a_.n_attacks_ == 0) {
			a_.n_attacks_ = a_.orig_attacks_;
			d_.n_attacks_ = d_.orig_attacks_;
			--rounds;
			defender_strikes_first = (d_stats_->firststrike && ! a_stats_->firststrike);
		}

		if (a_.n_attacks_ <= 0 && d_.n_attacks_ <= 0) {
			fire_event("attack_end");
			refresh_bc();
			break;
		}
	}

	// TODO: if we knew the viewing team, we could skip some of these display update
	if (update_def_fog_ && (*resources::teams)[defender_side - 1].uses_fog())
	{
		recalculate_fog(defender_side);
		if (update_display_) {
			resources::screen->invalidate_all();
			resources::screen->recalculate_minimap();
		}
	}

	if (update_minimap_ && update_display_) {
		resources::screen->recalculate_minimap();
	}

	if (a_.valid()) {
		unit &u = a_.get_unit();
		u.set_standing();
		u.set_experience(u.experience() + a_.xp_);
	}

	if (d_.valid()) {
		unit &u = d_.get_unit();
		u.set_standing();
		u.set_experience(u.experience() + d_.xp_);
	}

	unit_display::unit_sheath_weapon(a_.loc_,a_.valid()?&a_.get_unit():NULL,a_stats_->weapon,
			d_stats_->weapon,d_.loc_,d_.valid()?&d_.get_unit():NULL);

	if (update_display_){
		resources::screen->invalidate_unit();
		resources::screen->invalidate(a_.loc_);
		resources::screen->invalidate(d_.loc_);
		resources::screen->draw(true, true);
	}

	if(OOS_error_) {
		replay::process_error(errbuf_.str());
	}
}


int village_owner(const map_location& loc, const std::vector<team>& teams)
{
	for(size_t i = 0; i != teams.size(); ++i) {
		if(teams[i].owns_village(loc))
			return i;
	}

	return -1;
}

bool get_village(const map_location& loc, int side, int *action_timebonus)
{
	std::vector<team> &teams = *resources::teams;
	team *t = unsigned(side - 1) < teams.size() ? &teams[side - 1] : NULL;
	if (t && t->owns_village(loc)) {
		return false;
	}

	bool has_leader = resources::units->find_leader(side).valid();
	bool grants_timebonus = false;

	int old_owner_side = 0;
	// We strip the village off all other sides, unless it is held by an ally
	// and we don't have a leader (and thus can't occupy it)
	for(std::vector<team>::iterator i = teams.begin(); i != teams.end(); ++i) {
		int i_side = i - teams.begin() + 1;
		if (!t || has_leader || t->is_enemy(i_side)) {
			if(i->owns_village(loc)) {
				old_owner_side = i_side;
				i->lose_village(loc);
			}
			if (side != i_side && action_timebonus) {
				grants_timebonus = true;
			}
		}
	}

	if (!t) return false;

	if(grants_timebonus) {
		t->set_action_bonus_count(1 + t->action_bonus_count());
		*action_timebonus = 1;
	}

	if(has_leader) {
		if (resources::screen != NULL) {
			resources::screen->invalidate(loc);
		}
		return t->get_village(loc, old_owner_side);
	}

	return false;
}

// Simple algorithm: no maximum number of patients per healer.
void reset_resting(unit_map& units, int side)
{
	foreach (unit &u, units) {
		if (u.side() == side)
			u.set_resting(true);
	}
}

/* Contains all the data used to display healing */
struct unit_healing_struct {
	unit *healed;
	std::vector<unit *> healers;
	int healing;
};

void calculate_healing(int side, bool update_display)
{
	DBG_NG << "beginning of healing calculations\n";
	unit_map &units = *resources::units;

	std::list<unit_healing_struct> l;

	// We look for all allied units, then we see if our healer is near them.
	foreach (unit &u, units) {

		if (u.get_state("unhealable") || u.incapacitated())
			continue;

		DBG_NG << "found healable unit at (" << u.get_location() << ")\n";

		unit_map::iterator curer = units.end();
		std::vector<unit *> healers;

		int healing = 0;
		int rest_healing = 0;

		std::string curing;

		unit_ability_list heal = u.get_abilities("heals");

		const bool is_poisoned = u.get_state(unit::STATE_POISONED);
		if(is_poisoned) {
			// Remove the enemies' healers to determine if poison is slowed or cured
			for (std::vector<std::pair<const config *, map_location> >::iterator
					h_it = heal.cfgs.begin(); h_it != heal.cfgs.end();) {

				unit_map::iterator potential_healer = units.find(h_it->second);

				assert(potential_healer != units.end());
				if ((*resources::teams)[potential_healer->side() - 1].is_enemy(side)) {
					h_it = heal.cfgs.erase(h_it);
				} else {
					++h_it;
				}
			}
			for (std::vector<std::pair<const config *, map_location> >::const_iterator
					heal_it = heal.cfgs.begin(); heal_it != heal.cfgs.end(); ++heal_it) {

				if((*heal_it->first)["poison"] == "cured") {
					curer = units.find(heal_it->second);
					// Full curing only occurs on the healer turn (may be changed)
					if(curer->side() == side) {
						curing = "cured";
					} else if(curing != "cured") {
						curing = "slowed";
					}
				} else if(curing != "cured" && (*heal_it->first)["poison"] == "slowed") {
					curer = units.find(heal_it->second);
					curing = "slowed";
				}
			}
		}

		// For heal amounts, only consider healers on side which is starting now.
		// Remove all healers not on this side.
		for (std::vector<std::pair<const config *, map_location> >::iterator h_it =
				heal.cfgs.begin(); h_it != heal.cfgs.end();) {

			unit_map::iterator potential_healer = units.find(h_it->second);
			assert(potential_healer != units.end());
			if(potential_healer->side() != side) {
				h_it = heal.cfgs.erase(h_it);
			} else {
				++h_it;
			}
		}

		unit_abilities::effect heal_effect(heal,0,false);
		healing = heal_effect.get_composite_value();

		for(std::vector<unit_abilities::individual_effect>::const_iterator heal_loc = heal_effect.begin(); heal_loc != heal_effect.end(); ++heal_loc) {
			healers.push_back(&*units.find(heal_loc->loc));
		}

		if (!healers.empty()) {
			DBG_NG << "Unit has " << healers.size() << " potential healers\n";
		}

		if (u.side() == side) {
			unit_ability_list regen = u.get_abilities("regenerate");
			unit_abilities::effect regen_effect(regen,0,false);
			if(regen_effect.get_composite_value() > healing) {
				healing = regen_effect.get_composite_value();
				healers.clear();
			}
			if(!regen.cfgs.empty()) {
				for (std::vector<std::pair<const config *, map_location> >::const_iterator regen_it = regen.cfgs.begin(); regen_it != regen.cfgs.end(); ++regen_it) {
					if((*regen_it->first)["poison"] == "cured") {
						curer = units.end();
						curing = "cured";
					} else if(curing != "cured" && (*regen_it->first)["poison"] == "slowed") {
						curer = units.end();
						curing = "slowed";
					}
				}
			}
			if (int h = resources::game_map->gives_healing(u.get_location())) {
				if (h > healing) {
					healing = h;
					healers.clear();
				}
				/** @todo FIXME */
				curing = "cured";
				curer = units.end();
			}
			if (u.resting() || u.is_healthy()) {
				rest_healing = game_config::rest_heal_amount;
				healing += rest_healing;
			}
		}
		if(is_poisoned) {
			if(curing == "cured") {
				u.set_state(unit::STATE_POISONED, false);
				healing = rest_healing;
				healers.clear();
				if (curer != units.end())
					healers.push_back(&*curer);
			} else if(curing == "slowed") {
				healing = rest_healing;
				healers.clear();
				if (curer != units.end())
					healers.push_back(&*curer);
			} else {
				healers.clear();
				healing = rest_healing;
				if (u.side() == side) {
					healing -= game_config::poison_amount;
				}
			}
		}

		if (curing == "" && healing==0) {
			continue;
		}

		int pos_max = u.max_hitpoints() - u.hitpoints();
		int neg_max = -(u.hitpoints() - 1);
		if(healing > 0 && pos_max <= 0) {
			// Do not try to "heal" if HP >= max HP
			continue;
		}
		if(healing > pos_max) {
			healing = pos_max;
		} else if(healing < neg_max) {
			healing = neg_max;
		}

		if (!healers.empty()) {
			DBG_NG << "Just before healing animations, unit has " << healers.size() << " potential healers\n";
		}


		if (!recorder.is_skipping() && update_display &&
		    !(u.invisible(u.get_location()) &&
		      (*resources::teams)[resources::screen->viewing_team()].is_enemy(side)))
		{
			unit_healing_struct uhs = { &u, healers, healing };
			l.push_front(uhs);
		}
		if (healing > 0)
			u.heal(healing);
		else if (healing < 0)
			u.take_hit(-healing);
		resources::screen->invalidate_unit();
	}

	// Display healing with nearest first algorithm.
	if (!l.empty()) {

		// The first unit to be healed is chosen arbitrarily.
		unit_healing_struct uhs = l.front();
		l.pop_front();

		unit_display::unit_healing(*uhs.healed, uhs.healed->get_location(),
			uhs.healers, uhs.healing);

		/* next unit to be healed is nearest from uhs left in list l */
		while (!l.empty()) {

			std::list<unit_healing_struct>::iterator nearest;
			int min_d = INT_MAX;

			/* for each unit in l, remember nearest */
			for (std::list<unit_healing_struct>::iterator i =
			     l.begin(), i_end = l.end(); i != i_end; ++i)
			{
				int d = distance_between(uhs.healed->get_location(), i->healed->get_location());
				if (d < min_d) {
					min_d = d;
					nearest = i;
				}
			}

			uhs = *nearest;
			l.erase(nearest);

			unit_display::unit_healing(*uhs.healed, uhs.healed->get_location(),
				uhs.healers, uhs.healing);
		}
	}

	DBG_NG << "end of healing calculations\n";
}


unit get_advanced_unit(const unit &u, const std::string& advance_to)
{
	const unit_type *new_type = unit_types.find(advance_to);
	if (!new_type) {
		throw game::game_error("Could not find the unit being advanced"
			" to: " + advance_to);
	}
	unit new_unit(u);
	new_unit.set_experience(new_unit.experience() - new_unit.max_experience());
	new_unit.advance_to(new_type);
	new_unit.set_state(unit::STATE_POISONED, false);
	new_unit.set_state(unit::STATE_SLOWED, false);
	new_unit.set_state(unit::STATE_PETRIFIED, false);
	new_unit.set_user_end_turn(false);
	new_unit.set_hidden(false);
	return new_unit;
}

void advance_unit(map_location loc, const std::string &advance_to, const bool &fire_event)
{
	unit_map::unit_iterator u = resources::units->find(loc);
	if(!u.valid()) {
		return;
	}
	// original_type is not a reference, since the unit may disappear at any moment.
	std::string original_type = u->type_id();

	if(fire_event)
	{
		LOG_NG << "firing advance event at " << loc <<"\n";
		game_events::fire("advance",loc);

		if (!u.valid() || u->experience() < u->max_experience() ||
			u->type_id() != original_type)
		{
			LOG_NG << "WML has invalidated the advancing unit, abort\n";
			return;
		}
	}


	loc = u->get_location();
	unit new_unit = get_advanced_unit(*u, advance_to);
	statistics::advance_unit(new_unit);

	preferences::encountered_units().insert(new_unit.type_id());
	LOG_CF << "Added '" << new_unit.type_id() << "' to encountered units\n";

	resources::units->replace(loc, new_unit);
	if(fire_event)
	{
		LOG_NG << "firing post_advance event at " << loc << "\n";
		game_events::fire("post_advance",loc);
	}

	resources::whiteboard->on_gamestate_change();
}

int combat_modifier(const map_location &loc,
	unit_type::ALIGNMENT alignment, bool is_fearless)
{
	const time_of_day &tod = resources::tod_manager->get_illuminated_time_of_day(loc);

	int bonus;
	int lawful_bonus = tod.lawful_bonus;

	switch(alignment) {
		case unit_type::LAWFUL:
			bonus = lawful_bonus;
			break;
		case unit_type::NEUTRAL:
			bonus = 0;
			break;
		case unit_type::CHAOTIC:
			bonus = -lawful_bonus;
			break;
		case unit_type::LIMINAL:
			bonus = -(abs(lawful_bonus));
			break;
		default:
			bonus = 0;
	}

	if(is_fearless)
		bonus = std::max<int>(bonus, 0);

	return bonus;
}

namespace {

	/**
	 * Clears shroud from a single location.
	 *
	 * In a few cases, this will also clear corner hexes that otherwise would
	 * not normally get cleared.
	 * @param tm               The team whose fog/shroud is affected.
	 * @param loc              The location to clear.
	 * @param seen_units       If the location was cleared and contained a visible,
	 *                         non-petrified unit, it gets added to this set.
	 * @param petrified_units  If the location was cleared and contained a visible,
	 *                         petrified unit, it gets added to this set.
	 * @param known_units      These locations are excluded from being added to
	 *                         seen_units and petrified_units.
	 *                         If this is NULL, nothing gets stored in seen_units
	 *                         and petrified_units.
	 * @param invalidate       If set to true, the hexes will be marked for redrawing.
	 *
	 * @return true if the specified location was fogged or shrouded.
	 */
	bool clear_shroud_loc(team &tm, const map_location& loc,
	                      std::set<map_location>* seen_units = NULL,
	                      std::set<map_location>* petrified_units = NULL,
	                      const std::set<map_location>* known_units = NULL,
	                      bool invalidate = false)
	{
		gamemap &map = *resources::game_map;
		bool result = false;

		// We clear one past the edge of the board, so that the half-hexes
		// at the edge can also be cleared of fog/shroud.
		if ( map.on_board_with_border(loc)) {
			// Both functions should be executed so don't use || which
			// uses short-cut evaluation.
			result = tm.clear_shroud(loc) | tm.clear_fog(loc);

			if ( result ) {
				// Do not count clearing the map border as part of the return value.
				result = map.on_board(loc);

				// If we are near a corner, the corner might also need to be cleared.
				// This happens at the lower-left corner and at either the upper- or
				// lower- right corner (depending on the width).

				// Lower-left corner:
				if ( loc.x == 0  &&  loc.y == map.h()-1 ) {
					const map_location corner(-1, map.h());
					tm.clear_shroud(corner);
					tm.clear_fog(corner);
				}
				// Lower-right corner, odd width:
				else if ( is_odd(map.w())  &&  loc.x == map.w()-1  &&  loc.y == map.h()-1 ) {
					const map_location corner(map.w(), map.h());
					tm.clear_shroud(corner);
					tm.clear_fog(corner);
				}
				// Upper-right corner, even width:
				else if ( is_even(map.w())  &&  loc.x == map.w()-1  &&  loc.y == 0) {
					const map_location corner(map.w(), -1);
					tm.clear_shroud(corner);
					tm.clear_fog(corner);
				}

				// Possible screen invalidation.
				if ( invalidate ) {
					resources::screen->invalidate(loc);
					// Need to also invalidate adjacent hexes to get rid of the
					// "fog edge" graphics.
					map_location adjacent[6];
					get_adjacent_tiles(loc, adjacent);
					for ( int i = 0; i != 6; ++i )
						resources::screen->invalidate(adjacent[i]);
				}
			}
		}

		// Does the caller want a list of discovered units?
		if ( result  &&  known_units  &&  (seen_units || petrified_units) ) {
			// Allow known_units to override fogged().
			if ( known_units->count(loc) == 0 ) {
				// Is there a visible unit here?
				const unit_map::const_iterator sighted = resources::units->find(loc);
				if ( sighted.valid() ) {
					if ( !tm.is_enemy(sighted->side()) ||
					     !sighted->invisible(loc) )
					{
						// Add this unit to the appropriate list.
						if ( !sighted->get_state(unit::STATE_PETRIFIED) )
						{
							if ( seen_units != NULL )
								seen_units->insert(loc);
						}
						else if ( petrified_units != NULL )
							petrified_units->insert(loc);
					}
				}
			}
		}

		return result;
	}

	/**
	 * Clears shroud (and fog) around the provided location for @a view_team as
	 * if a unit with @a viewer's sight range was standing there.
	 * (This uses a team parameter instead of a side since it is assumed that
	 * the caller already checked for fog or shroud being in use. Hence the
	 * caller has the team readily available.)
	 *
	 * @a seen_units will return new units that have been seen by this unit.
	 * If @a known_units is NULL, @a seen_units will not be changed (so might as
	 * well also be NULL).
	 * If @ known_units is not NULL, but does not contain @ viewer's actual
	 * location, then it could be possible for a unit to see itself in the case
	 * where @ viewer managed to end up on a fogged/shrouded location.
	 *
	 * @returns true if some shroud (or fog) is cleared.
	 */
	bool clear_shroud_unit(const map_location &view_loc, const unit &viewer,
	                       team &view_team, const std::map<map_location, int>& jamming_map,
	                       const std::set<map_location>* known_units = NULL,
	                       std::set<map_location>* seen_units = NULL,
	                       std::set<map_location>* petrified_units = NULL,
	                       bool invalidate_display = false)
	{
		bool cleared_something = false;

		// Clear the fog.
		pathfind::vision_path sight(*resources::game_map, viewer, view_loc, jamming_map);
		foreach (const pathfind::paths::step &dest, sight.destinations) {
			if ( clear_shroud_loc(view_team, dest.curr, seen_units, petrified_units, known_units, invalidate_display) )
				cleared_something = true;
		}
		//TODO guard with game_config option
		foreach (const map_location &dest, sight.edges) {
			if ( clear_shroud_loc(view_team, dest, seen_units, petrified_units, known_units, invalidate_display) )
				cleared_something = true;
		}

		return cleared_something;
	}

}

void calculate_jamming(int side, std::map<map_location, int>& jamming_map)
{
	team& viewer_tm = (*resources::teams)[side - 1];

	foreach (const unit &u, *resources::units)
	{
		if (!viewer_tm.is_enemy(u.side())) continue;
		if (u.jamming() < 1) continue;

		int current = jamming_map[u.get_location()];
		if (current < u.jamming()) jamming_map[u.get_location()] = u.jamming();

		pathfind::jamming_path jamming(*resources::game_map, u, u.get_location());
		foreach (const pathfind::paths::step& st, jamming.destinations) {
			current = jamming_map[st.curr];
			if (current < st.move_left) jamming_map[st.curr] = st.move_left;
		}

	}
}

/**
 * Function that recalculates the fog of war.
 *
 * This is used at the end of a turn and for the defender at the end of
 * combat. As a back-up, it is also called when clearing shroud at the
 * beginning of a turn.
 * This function does nothing if the indicated side does not use fog.
 *
 * @param[in] side The side whose fog will be recalculated.
 */
void recalculate_fog(int side)
{
	team &tm = (*resources::teams)[side - 1];

	if (!tm.uses_fog())
		return;

	tm.refog();

	std::map<map_location, int> jamming_map;
	calculate_jamming(side, jamming_map);
	foreach (const unit &u, *resources::units)
	{
		if (u.side() == side) {
			clear_shroud_unit(u.get_location(), u, tm, jamming_map);
		}
	}

	//FIXME: This pump don't catch any sighted events (they are not fired by
	// clear_shroud_unit) and if it caches another old event, maybe the caller
	// don't want to pump it here
	game_events::pump();
}

/**
 * Function that will clear shroud (and fog) based on current unit positions.
 *
 * This will not re-fog hexes unless reset_fog is set to true.
 * This function will do nothing if the side uses neither shroud nor fog.
 *
 * @param[in] side      The side whose shroud (and fog) will be cleared.
 * @param[in] reset_fog If set to true, the fog will also be recalculated
 *                      (refogging hexes that can no longer be seen).
 * @returns true if some shroud/fog is actually cleared away.
 */
bool clear_shroud(int side, bool reset_fog)
{
	team &tm = (*resources::teams)[side - 1];
	if (!tm.uses_shroud() && !tm.uses_fog())
		return false;

	bool result = false;

	std::map<map_location, int> jamming_map;
	calculate_jamming(side, jamming_map);
	foreach (const unit &u, *resources::units)
	{
		if (u.side() == side) {
			result |= clear_shroud_unit(u.get_location(), u, tm, jamming_map);
		}
	}

	//FIXME: This pump don't catch any sighted events (they are not fired by
	// clear_shroud_unit) and if it caches another old event, maybe the caller
	// don't want to pump it here
	game_events::pump();

	if ( reset_fog ) {
		recalculate_fog(side);
	}

	resources::screen->labels().recalculate_labels();
	resources::screen->labels().recalculate_shroud();

	return result;
}

namespace { // Helper functions for move_unit()

/// Various flags and data indicating why movement was unexpectedly stopped early.
struct movement_surprises {
	// Reasons for stopping:
	bool ambush_stop;
	bool sighted_stop;
	bool teleport_failed;
	bool event_stop;
	bool event_mutated;
	// Information uncovered:
	bool display_changed;
	bool sighted_something;
	bool block_undo;	// Combines some earlier flags, plus a few other cases.
	// Some associated data:
	std::string ambushed_string;
	std::vector<map_location> ambushers;
	std::set<map_location> seen_units;

	/// Constructor:
	movement_surprises () :
		ambush_stop(false),
		sighted_stop(false),
		teleport_failed(false),
		event_stop(false),
		event_mutated(false),
		display_changed(false),
		sighted_something(false),
		block_undo(false),
		ambushed_string(),
		ambushers(),
		seen_units()
	{}
};

/// Handles animating unit movement in controllable stages
class unit_mover
{
	unit_map& m_;
	bool movement_started_;
	const bool show_move_;
	const std::vector<map_location> &route_;
	std::vector<map_location>::const_iterator loc_;
	map_location displaced_;

	/**
	 * Places a temporary unit on the display when constructed,
	 * and removes it when destructed.  The real unit will be
	 * hidden during that period.
	 */
	class animation_unit_placer
	{
		unit &ui_;
		bool was_hidden_;

	public:
		/// The temporary unit available for use while this class
		/// is in scope
		game_display::fake_unit temp_unit;

		/**
		 * Creates a temporary unit, hides the real unit, and places
		 * the temporary unit on the display.
		 *
		 * @param ui[in]	The real unit to base the temporary unit on
		 */
		animation_unit_placer(unit &ui) :
			ui_(ui),
			was_hidden_(ui.get_hidden()),
			temp_unit(ui)
		{
			ui_.set_hidden(true);
			temp_unit.set_standing(false);
			temp_unit.set_hidden(false);
			temp_unit.place_on_game_display(resources::screen);
		}

		/**
		 * Removes the temporary unit from the display,
		 * and returns the real unit that it was based
		 * on to its previous hidden/shown state.
		 */
		~animation_unit_placer()
		{
			temp_unit.remove_from_game_display();
			ui_.set_hidden(was_hidden_);
		}
	};

	/**
	 * Empties a tile by displacing the occupying
	 * unit to a nearby tile. This can be reversed
	 * by a subsequent call to undisplace().
	 *
	 * This fails silently if there is no acceptable
	 * tile to move the unit to.
	 */
	void displace(const map_location &from) {
		game_display &disp = *resources::screen;

		unit_map::iterator displaced_unit = m_.find(from);
		if(displaced_unit != m_.end()) {
			displaced_ = pathfind::find_vacant_tile(
				*resources::game_map,
				m_,
				from,
				pathfind::VACANT_ANY,
				&*displaced_unit);
			m_.move(from, displaced_);

			disp.invalidate_unit_after_move(from, displaced_);
			disp.invalidate(from);
			disp.invalidate(displaced_);
			m_.find(displaced_)->set_standing();
		}
	}

	/**
	 * Returns the displaced unit to the tile it came from.
	 *
	 * This fails silently if the tile is already occupied.
	 */
	void undisplace() {
		game_display &disp = *resources::screen;

		if(displaced_ != map_location::null_location)
		{
			m_.move(displaced_, *loc_);

			disp.invalidate_unit_after_move(displaced_, *loc_);
			disp.invalidate(displaced_);
			disp.invalidate(*loc_);
			m_.find(*loc_)->set_standing();

			displaced_ = map_location::null_location;
		}
	}
public:
	/**
	 * Constructor
	 *
	 * @param route[in]      the path to follow
	 * @param show_move[in]  whether or not the move is shown to the player
	 */
	unit_mover(const std::vector<map_location> &route, bool show_move) :
		m_(*resources::units),
		movement_started_(false),
		show_move_(show_move),
		route_(route),
		loc_(route.begin()),
		displaced_(map_location::null_location)
	{}

	/**
	 * Displays the unit's starting animation, if necessary.
	 * This will be called by continue_movement() as needed.
	 */
	void start_movement()
	{
		if(movement_started_)
			return;
		movement_started_ = true;

		std::vector<team> &teams = *resources::teams;

		unit_map::iterator ui = m_.find(*loc_);
		if(ui == m_.end())
			return;

		if(show_move_)
		{
			// show the movement start animation
			team &tm = teams[ui->side() - 1];

			animation_unit_placer placer(*ui);

			unit_display::move_unit_start(route_, placer.temp_unit, tm);
		}
	}

	/**
	 * Moves the unit to a later position in the route. This
	 * should usually by the next tile, but does not have to be.
	 *
	 * @param dest[in]  An iterator into the route provided to the constructor,
	 *                  indicating which location to move the unit to.
	 *                  This location must always be later in the route than
	 *                  the unit's current position.
	 */
	void continue_movement(const std::vector<map_location>::const_iterator dest)
	{
		start_movement();

		std::vector<team> &teams = *resources::teams;
		game_display &disp = *resources::screen;

		assert(loc_ < dest);
		assert(dest < route_.end());

		unit *ui = m_.extract(*loc_);

		undisplace();

		if(ui && show_move_)
		{
			// show the movement animation
			team &tm = teams[ui->side() - 1];

			animation_unit_placer placer(*ui);

			for(std::vector<map_location>::const_iterator step = loc_; step != dest; ++step) {
				unit_display::move_unit_step(route_, step - route_.begin(), placer.temp_unit, tm);
			}
		}
		if(ui)
		{
			displace(*dest);

			// move the real unit
			ui->set_location(*dest);
			m_.insert(ui);
			ui->set_facing((dest-1)->get_relative_dir(*dest));
			ui->set_standing();
			disp.invalidate_unit_after_move(*loc_, *dest);
			disp.invalidate(*loc_);
			disp.invalidate(*dest);
		}

		loc_ = dest;
	}

	/**
	 * Ends the unit's animation, displaying the unit's movement
	 * end animation if necessary and returning any displaced unit
	 * to the field.
	 */
	~unit_mover()
	{
		if(!movement_started_)
			return;

		unit_map::iterator ui = m_.find(*loc_);

		if(ui != m_.end() && show_move_)
		{
			// show the movement finish animation
			animation_unit_placer placer(*ui);

			std::vector<map_location> steps(route_.begin(),loc_+1);
			unit_display::move_unit_finish(steps, placer.temp_unit);
		}

		if(m_.count(*loc_) == 0)
		{
			// Apparently the moving unit was removed by an event
			// while over an occupied tile.  Put the displaced
			// unit back where it was.
			undisplace();
		}
	}
};


/**
 * Determines how far along a route a unit can move this turn, assuming no surprises.
 *
 * This function is used by move_unit() to calculate how far a unit should
 * believe it can move this turn. This way, a player cannot, for example,
 * plot movement through a crowd that the selected unit cannot clear this
 * turn, then by executing that plot, benefit from the unit's sighting range
 * towards the front of the crowd even though the player knows the unit
 * cannot reach that far (because it cannot stop on another unit).
 *
 * @param[in]      route         The plotted path. Assumed not empty.
 * @param[in]      mover_it      The unit that will move along the path.
 * @param[in,out]  moves_left    The movement points the unit has remaining.
 *                               This does not account for things that set movement
 *                               to zero (such as entering a zone of control).
 * @param[out]     zoc_stop      The map location at which the unit enters an
 *                               enemy's zone of control. May be the null location.
 * @param[in]      ignore_limit  Set to true to allow the unit to exceed its
 *                               movement limit. (Used for replays.)
 * @returns An iterator (into @a route) pointing one past where the unit should stop.
 * @retval route.begin() if the unit cannot move (rather then route.begin()+1).
 *                       @a moves_left may be set to bogus data in this case.
 */
std::vector<map_location>::const_iterator known_movement_end(
	const std::vector<map_location> &route, const unit_map::const_iterator &mover_it,
	int &moves_left, map_location &zoc_stop, bool ignore_limit)
{
	// Initialize output.
	zoc_stop = map_location::null_location;

	// Alias some resources.
	const gamemap &map = *resources::game_map;
	const unit_map &units = *resources::units;
	const int current_side = mover_it->side();
	const team &current_team = (*resources::teams)[current_side-1];

	// This loop will end with step pointing one element beyond
	// where the unit thinks it will stop.
	std::vector<map_location>::const_iterator step;
	for ( step = route.begin()+1; step != route.end(); ++step) {
		// Check if we needed to stop in the previous hex.
		if ( zoc_stop != map_location::null_location ) {
			break;
		}

		// Check that we have enough movement for this step.
		const int cost = mover_it->movement_cost(map[*step]);
		if ( cost > moves_left && !ignore_limit ) {
			break;
		}

		// At this point we can enter this hex.
		// Calculate the remaining moves.
		moves_left -= cost;

		// Check for being unable to leave this hex.
		if ( !mover_it->get_ability_bool("skirmisher", *step)  &&
		      pathfind::enemy_zoc(current_team, *step, current_team) ) {
			zoc_stop = *step;
		}
	}

	// Avoiding stopping on a (known) unit.
	while ( step != route.begin()  &&  units.end() != find_visible_unit(*(step-1), current_team) ) {
		// Backtrack.
		moves_left += mover_it->movement_cost(map[*(--step)]);
	}

	return step;
}


/**
 * Determines how far along a route a unit can move without encountering surprises.
 *
 * This function is used by move_unit() to determine if movement is ended early
 * due to surprises (like ambushes or sighted units). It assumes the given interval
 * of the route has already been checked for known factors (like movement costs).
 *
 * @param[in]      route_begin   The beginning of the intended path.
 * @param[in]      route_end     The end of the intended path (one past the last
 *                               hex). Assumed to not be route_begin.
 * @param[in]      mover_it      The unit that will move along the path.
 * @param[in,out]  moves_left    The movement points the unit has remaining.
 *                               This does not account for things that set
 *                               movement to zero (such as being ambushed).
 * @param[in,out]  flags         For returning data to the caller. (It is "in"
 *                               only because the caller is responsible for
 *                               initializing this with the default constructor.)
 * @param[in]      fog_shroud    Set to true to cause fog and shroud to be
 *                               cleared along the path (up to where we stop).
 * @param[in]      skip_sight    Set to true to avoid looking for units as the
 *                               fog/shroud is cleared. (Used for continued moves.)
 * @param[in]      ignore_invis  Set to true to avoid stopping movement due to
 *                               revealed units. (Used for replays.)
 * @param[out]     spectator     If not NULL, this will be told which unit
 *                               ambushed, or blocked the teleport of, mover_it.
 *
 * @returns An iterator (into the provided interval) pointing one past where the
 *          unit should stop.
 * @retval route.begin() if the unit cannot move (rather then route.begin()+1).
 *                       @a moves_left may be set to bogus data in this case.
 */
std::vector<map_location>::const_iterator surprise_movement_end(
	const std::vector<map_location>::const_iterator &route_begin,
	const std::vector<map_location>::const_iterator &route_end,
	const unit_map::const_iterator &mover_it, int &moves_left,
	movement_surprises &flags, bool ignore_invis,
	move_unit_spectator *spectator)
{
	// Alias some resources.
	const gamemap &map = *resources::game_map;
	unit_map &units = *resources::units;
	const int current_side = mover_it->side();
	team &current_team = (*resources::teams)[current_side-1];
	game_display &disp = *resources::screen;

	// This loop will end with step pointing one element beyond
	// where the unit will stop.
	std::vector<map_location>::const_iterator step;
	for ( step = route_begin+1; step != route_end; ++step) {
		// Check if we need to stop in the previous hex.
		if ( flags.ambush_stop ) {
			if ( !ignore_invis )
				break;
		}

		// Check for being unable to enter this hex.
		const unit_map::const_iterator blocking_unit = units.find(*step);
		if ( blocking_unit != units.end() ) {
			// Was this teleportation?
			if ( !tiles_adjacent(*(step-1), *step) ) {
				// Cannot teleport to an occupied hex.
				if ( spectator != NULL )
					spectator->set_failed_teleport(blocking_unit);
				flags.teleport_failed = true;
				break;
			// Else, make sure we do not go through an enemy.
			} else if ( current_team.is_enemy(blocking_unit->side()) ) {
				// (Missed an ambusher? Pathfinding bug?)
				disp.invalidate(*step);
				if ( spectator != NULL )
					spectator->set_ambusher(blocking_unit);
				flags.block_undo = true;
				break;
			}
		}

		// See if we are stopped in this hex.
		// First check: something adjacent might stop us.
		map_location adjacent[6];
		get_adjacent_tiles(*step, adjacent);
		for ( int i = 0; i != 6; ++i ) {
			// Look for a hidden enemy (an ambusher).
			const unit_map::iterator neighbor_it = units.find(adjacent[i]);
			if ( units.end() != neighbor_it &&
			     current_team.is_enemy(neighbor_it->side()) &&
			     neighbor_it->invisible(neighbor_it->get_location()) ) {
				// Ambushed!
				flags.ambushers.push_back(adjacent[i]);
				flags.ambush_stop = true;
				if ( spectator!=NULL )
					spectator->set_ambusher(neighbor_it);

				// Get a feedback message (first one available).
				const unit_ability_list hides = neighbor_it->get_abilities("hides");
				std::vector<std::pair<const config *, map_location> >::const_iterator hide_it;
				for ( hide_it = hides.cfgs.begin();
				      hide_it != hides.cfgs.end() && flags.ambushed_string.empty();
				      ++hide_it )
				{
					flags.ambushed_string = (*hide_it->first)["alert"].str();
				}
			}
		}
	}//for (loop through expected move for this turn)

	// Consolidate all the cases where undo should be blocked.
	flags.block_undo =
		flags.block_undo || flags.ambush_stop || flags.display_changed ||
		flags.teleport_failed || flags.sighted_something; // sighted_something should be implied by display_changed, but why assume?
	// Default ambush message?
	if ( flags.ambush_stop && flags.ambushed_string.empty() )
		flags.ambushed_string = _("Ambushed!");

	// Oddball cases could mean we got stopped on an occupied hex.
	// (e.g. ambushed on a hex occupied by an ally of both us and the ambusher)
	// Avoiding stopping on a unit.
	while ( step != route_begin  &&  units.count(*(step-1)) > 0 ) {
		// Backtrack.
		--step;
	}

	// Credit movement for hexes not entered.
	for ( std::vector<map_location>::const_iterator skipping = step; skipping != route_end; ++skipping ) {
		moves_left += mover_it->movement_cost(map[*skipping]);
	}

	return step;
}


/**
 * Performs a move, once the move is determined "allowable".
 *
 * In the process, updates unit movement and shroud, and fires
 * exit_hex, sighted and enter_hex events (in that order).  No
 * checks are made to prevent the unit from moving past or through
 * enemy units.
 *
 * @param steps[in]           The path to follow
 * @param show[in]            Whether or not the move is shown to the player
 * @param flags[in,out]       For returning data to the caller. (It is "in"
 *                            only because the caller is responsible for
 *                            initializing this with the default constructor.)
 * @param fog_shroud[in]      Set to true to cause fog and shroud to be
 *                            cleared along the path (up to where we stop).
 * @param skip_sight[in]      Set to true to avoid looking for units as the
 *                            fog/shroud is cleared. (Used for continued moves.)
 * @param empty_movement[in]  Whether or not to set the unit's movement
 *                            points to zero at the end of the move
 *
 * @returns An iterator (into the provided interval) pointing one past where the
 *	    unit stopped.
 */
std::vector<map_location>::const_iterator make_a_move(const std::vector<map_location> &steps, bool show,
	movement_surprises &flags, bool fog_shroud, bool skip_sight, bool empty_movement)
{
	// Some convenient aliases:
	gamemap &map = *resources::game_map;
	unit_map &units = *resources::units;
	game_display &disp = *resources::screen;

	std::string exit_hex_str("exit_hex");
	std::string enter_hex_str("enter_hex");

	// Store the moving side. If WML changes the unit's side,
	// finish the move on behalf of the original side.
	const int current_side = units.find(steps.front())->side();
	team &current_team = (*resources::teams)[current_side-1];

	// The set of units currently seen by the moving side:
	std::set<map_location> known_units;
	// (Only needed if we will be clearing fog/shroud.)
	if ( fog_shroud ) {
		foreach (const unit &u, units) {
			if ( u.side() == current_side  ||  !current_team.fogged(u.get_location()) )
				known_units.insert(u.get_location());
		}
	}

	unit_mover mover(steps, show);

	std::vector<map_location>::const_iterator
		from = steps.begin(),
		to,
		stopped = steps.end();

	for(to = from + 1; to != steps.end(); ++from, ++to) {
		game_events::raise(exit_hex_str, *from, *to);
		if(game_events::pump() || units.find(*from) == units.end()) {
			stopped = from;
			break;
		}

		bool sighted_can_stop = false;
		if ( fog_shroud ) {
			// We can reasonably stop if the hex is empty and not an unowned village.
			// We also don't "stop" if already at the end of the route.
			sighted_can_stop = units.count(*to) == 0 &&
					   (!map.is_village(*to) || current_team.owns_village(*to)) &&
					   to+1 != steps.end();
		}

		unit_map::iterator ui = units.find(*from);
		int moves_left = ui->movement_left() - ui->movement_cost(map[*to]);
		if(moves_left < 0) {
			stopped = from;
			break;
		}
		mover.continue_movement(to);
		if(empty_movement && to+1 == steps.end()) {
			moves_left = 0;
		}
		ui->set_movement(moves_left);

		// We can enter this hex. Take a look around.
		if ( fog_shroud ) {
			DBG_NG << "Checking for units from " << (to->x+1) << "," << (to->y+1) << ".\n";

			std::set<map_location> seen_units;
			std::set<map_location> petrified_units;

			// Clear the fog/shroud.
			std::map<map_location, int> jamming_map;
			calculate_jamming(current_side, jamming_map);
			flags.display_changed |=
					clear_shroud_unit(*to, *ui, current_team, jamming_map, &known_units,
					                  &seen_units, &petrified_units, true);

			{	// Fire sighted events
				std::set<map_location>::iterator sight_it;
				const std::string sighted_str("sighted");
				const map_location loc = ui->get_location();
				foreach (const map_location &here, seen_units) {
					game_events::raise(sighted_str, here, loc);
					flags.seen_units.insert(here);
				}
				foreach (const map_location &here, petrified_units) {
					game_events::raise(sighted_str, here, loc);
				}
				// Pump these events at the same time as enter_hex
			}

			if ( !skip_sight && !flags.sighted_something ) {
				// Check whether a unit was sighted and should interrupt movement.
				if ( preferences::interrupt_when_ally_sighted() ) {
					// Interrupt if any unit was sighted.
					flags.sighted_something = !flags.seen_units.empty();
				} else {
					// Check whether any sighted unit is an enemy
					std::set<map_location>::const_iterator it;
					for ( it = flags.seen_units.begin(); it != flags.seen_units.end(); ++it ) {
						if ( current_team.is_enemy(units.find(*it)->side()) ) {
							flags.sighted_something = true;
							break;
						}
					}
				}
			}
		}//if (fog_shroud)

		game_events::raise(enter_hex_str, *to, *from);
		if(game_events::pump() || units.find(*to) == units.end()) {
			stopped = to;
			break;
		}

		// Check if we need to stop due to a unit sighting
		if(fog_shroud && flags.sighted_something && sighted_can_stop) {
			flags.sighted_stop = true;
			stopped = to;
			break;
		}
	}

	if(stopped != steps.end() && !flags.sighted_stop) {
		flags.event_mutated = true;
		flags.event_stop = (stopped+1 != steps.end());
	}

	if(stopped == steps.end())
		--stopped;

	// Invalidate adjacent hexes in case they contain invisible units
	// that will be revealed.
	map_location adjacent[6];
	get_adjacent_tiles(*stopped, adjacent);
	for ( int i = 0; i != 6; ++i ) {
		disp.invalidate(adjacent[i]);
	}

	// Update the unit's status.
	unit::clear_status_caches();

	return stopped;
}


/**
 * Handles the WML events triggered by movement (includes village capturing).
 *
 * @param initial_loc[in]          The starting location of the movement (needed for "moveto").
 * @param final_loc[in]            The final location of the movement (needed for everything).
 * @param stops[in]                The flags raised while moving (needed for "sighted").
 * @param fog_shroud[in]           Whether or not fog/shroud should be checked.
 * @param orig_village_owner[out]  If the final location is a village and the sighted
 *                                 event do not remove the capturer, this gets set to
 *                                 the side that owned the village before this movement.
 *                                 (Otherwise it remains unchanged.)
 * @param action_time_bonus[out]   If a village capture occurs, this gets set to 1.
 *                                 (Otherwise it remains unchanged.)
 *
 * @returns true if events might have changed the game state.
 */
bool movement_events(const map_location &initial_loc, const map_location &final_loc,
		     int &orig_village_owner, int &action_time_bonus)
{
	// Alias some resources.
	const gamemap &map = *resources::game_map;
	unit_map &units = *resources::units;
	const std::vector<team> &teams = *resources::teams;

	bool event_mutated = false;

	// Village capturing.
	if ( map.is_village(final_loc) && units.count(final_loc) != 0 ) { // Sighted events might have affected the moving unit.
		// Is this a capture?
		const unit_map::iterator captor = units.find(final_loc);
		const int captor_side = captor->side();

		orig_village_owner = village_owner(final_loc, teams);
		if ( orig_village_owner != captor_side-1 ) {
			// Captured. Zap movement and take over the village.
			captor->set_movement(0);
			event_mutated |= get_village(final_loc, captor_side, &action_time_bonus);
		}
	}

	// Moveto event.
	event_mutated |= game_events::fire("moveto", final_loc, initial_loc);

	// Done.
	return event_mutated;
}


/**
 * Shows messages (on the screen) as a result of movement.
 *
 * @param stops[in]         The flags raised while moving.
 * @param cut_short[in]		true if movement did not make it to the end of the plotted path.
 * @param current_team[in]	The moving team.
 * @param spectator[out]	Told of seen friends and enemies.
 */
void movement_feedback(const movement_surprises &stops, bool move_cut_short,
                       const team &current_team, move_unit_spectator *spectator)
{
	// Alias some resources.
	const unit_map &units = *resources::units;
	game_display &disp = *resources::screen;

	bool redraw = false;
	bool playing_team_is_viewing = disp.playing_team() == disp.viewing_team()
	                               ||  disp.show_everything();

	// Multiple messages may be displayed simultaneously
	// this variable is used to keep them from overlapping
	std::string message_prefix = "";

	// Ambush feedback?
	if ( stops.ambush_stop && !stops.event_stop) {
		// Suppress the message for observers if the ambusher(s) cannot be seen.
		bool show_message = playing_team_is_viewing;
		foreach (const map_location &ambush, stops.ambushers) {
			if ( !disp.fogged(ambush) )
				show_message = true;
		}
		if ( show_message ) {
			disp.announce(message_prefix+stops.ambushed_string, font::BAD_COLOR);
			message_prefix += " \n";
			redraw = true;
		}
	}

	// Failed teleport feedback?
	if ( playing_team_is_viewing  &&  stops.teleport_failed && !stops.event_stop ) {
		std::string teleport_string = _("Failed teleport! Exit not empty");
		disp.announce(message_prefix+teleport_string, font::BAD_COLOR);
		message_prefix += " \n";
		redraw = true;
	}

	// Sighted units feedback?
	if ( playing_team_is_viewing  &&  !stops.seen_units.empty() ) {
		// Count the number of allies and enemies sighted.
		int nfriends = 0, nenemies = 0;
		foreach (const map_location &loc, stops.seen_units) {
			DBG_NG << "Processing unit at " << loc << "...\n";
			const unit_map::const_iterator u = units.find(loc);

			// Unit may have been removed by an event.
			if ( u == units.end() ) {
				DBG_NG << "...was removed.\n";
				continue;
			}

			if ( current_team.is_enemy(u->side()) ) {
				++nenemies;
				if ( spectator != NULL )
					spectator->add_seen_enemy(u);
			} else {
				++nfriends;
				if ( spectator != NULL )
					spectator->add_seen_friend(u);
			}

			DBG_NG << "...processed.\n";
		}

		// Create the message to display (depends on whether firends,
		// enemies, or both were sigthed, and on how many of each).
		utils::string_map symbols;
		symbols["friends"] = lexical_cast<std::string>(nfriends);
		symbols["enemies"] = lexical_cast<std::string>(nenemies);
		std::string message;
		SDL_Color msg_color;
		if ( nfriends > 0  &&  nenemies > 0 ) {
			// Both friends and enemies sighted -- neutral message.
			symbols["friendphrase"] = vngettext("Part of 'Units sighted! (...)' sentence^1 friendly", "$friends friendly", nfriends, symbols);
			symbols["enemyphrase"] = vngettext("Part of 'Units sighted! (...)' sentence^1 enemy", "$enemies enemy", nenemies, symbols);
			message = vgettext("Units sighted! ($friendphrase, $enemyphrase)", symbols);
			msg_color = font::NORMAL_COLOR;
		} else if ( nfriends > 0 ) {
			// Only friends sighted -- good message.
			message = vngettext("Friendly unit sighted", "$friends friendly units sighted", nfriends, symbols);
			msg_color = font::GOOD_COLOR;
		} else if ( nenemies > 0 ) {
			// Only enemies sighted -- bad message.
			message = vngettext("Enemy unit sighted!", "$enemies enemy units sighted!", nenemies, symbols);
			msg_color = font::BAD_COLOR;
		}

		disp.announce(message_prefix+message, msg_color);
		message_prefix += " \n";
		redraw = true;
	}

	// Suggest "continue move"?
	if ( move_cut_short && stops.sighted_something && !stops.ambush_stop && !stops.event_stop && !resources::whiteboard->is_executing_actions() ) {
		// See if the "Continue Move" action has an associated hotkey
		const hotkey::hotkey_item& hk = hotkey::get_hotkey(hotkey::HOTKEY_CONTINUE_MOVE);
		if(!hk.null()) {
			utils::string_map symbols;
			symbols["hotkey"] = hk.get_name();
			std::string message = vgettext("(press $hotkey to keep moving)", symbols);
			disp.announce(message_prefix+message, font::NORMAL_COLOR);
			message_prefix += " \n";
			redraw = true;
		}
	}

	// Update the screen.
	if ( redraw )
		disp.draw();
}

}//end anonymous namespace

/** Moves a unit on the board.
 *
 * This function handles actual movement, checking terrain costs as well as
 * things that might interrupt movement (e.g. ambushes). If the full path
 * cannot be reached this turn, the remainder is stored as the unit's "goto"
 * instruction. (The unit itself is whatever unit is at the beginning of the
 * supplied path.)
 *
 * @param move_spectator[in,out]    Will be given information uncovered by the move. If not NULL, this suppresses all changes to the unit's "goto" instruction.
 * @param route[in]                 The route to be travelled. The unit to be moved is at the beginning of the route.
 * @param move_recorder[out]        Will be given the route actually travelled (which might be shorter than the route specified).
 * @param undo_stack[in,out]        If supplied, then either this movement will be added to the stack or the stack will be cleared.
 * @param show_move[in]             Controls whether or not the movement is animated for the player.
 * @param next_unit[out]            If supplied, this is set to where the actual movement ended. (Set to the null location if @a route is empty.)
 * @param continue_move[in]         If set to true, movement is not interrupted (will continue) should units be spotted.
 * @param should_clear_shroud[in]   If set to false, no fog/shroud clearing will occur. If left as true, then clearing depends upon the team's setting (delayed shroud updates).
 * @param is_replay[in]             If set to true, certain considerations (ambushes, spotted units, and running out of movement) are ignored (in order to preserve the replay).
 * @param units_sighted_result[out] Returns whether or not any (non-petrified) units were seen as a result of this move clearing fog/shroud.
 *
 * @returns The length of the path actually travelled
 *          (one more than the number of hexes entered).
 * @retval 0 if the movement degenerated to staying put.
 */
size_t move_unit(move_unit_spectator *move_spectator,
		 const std::vector<map_location> &route,
		 replay* move_recorder, undo_list* undo_stack,
		 bool show_move,
		 map_location *next_unit, bool continue_move,
		 bool should_clear_shroud, bool is_replay,
		 bool* units_sighted_result)
{
	const events::command_disabler disable_commands;
	const map_location &initial_loc = route.empty() ?
						map_location::null_location :
						route.front();

	// Default return values.
	if ( units_sighted_result )
		*units_sighted_result = false;
	if ( next_unit )
		*next_unit = initial_loc;

	// Avoid some silliness.
	if ( route.size() < 2  ||  (route.size() == 2 && route.front() == route.back()) ) {
		DBG_NG << "Ignoring a unit trying to jump on its hex at " << initial_loc << ".\n";
		return 0;
	}

	// Alias some resources.
	unit_map &units = *resources::units;
	game_display &disp = *resources::screen;

	// Locate the unit that is moving.
	unit_map::iterator ui = units.find(initial_loc);
	assert(ui != units.end());
	const int starting_moves = ui->movement_left();
	const map_location::DIRECTION orig_dir = ui->facing();

	// Cache the unit's team.
	const size_t team_num = ui->side() - 1;
	const team &tm = (*resources::teams)[team_num];
	// Update should_clear_shroud with the team's settings.
	should_clear_shroud = should_clear_shroud && tm.fog_or_shroud() && tm.auto_shroud_updates();

	// Default "goto" instruction.
	if ( move_spectator == NULL )
		ui->set_goto(route.back());

	// See how far along the given path we believe we can move this turn.
	int moves_left = starting_moves;
	map_location zoc_stop;
	std::vector<map_location>::const_iterator step =
		known_movement_end(route, ui, moves_left, zoc_stop, is_replay);
	// Check for having nowhere to go.
	if ( route.begin() == step )
		return 0;

	// See how far along the planned path we can actually move.
	movement_surprises stops;
	step = surprise_movement_end(route.begin(), step, ui, moves_left, stops,
				     is_replay, move_spectator);

	// Create a copy of the subroute we can traverse.
	std::vector<map_location> steps(route.begin(), step);
	map_location final_loc = initial_loc;

	if ( steps.size() >= 2 ) {
		// Actually move the unit.
		const std::vector<map_location>::const_iterator stopped = make_a_move(steps, show_move, stops, should_clear_shroud, continue_move || is_replay, stops.ambush_stop || zoc_stop == steps.back());
		ui = units.find(*stopped);
		final_loc = *stopped;

		// If an event stopped movement, then the replay needs to
		// know where the unit tried to go, to trigger the same events.
		// However, if a sighted event stopped movement, then
		// record only the movement that actually happened.
		if (stops.sighted_stop)
			steps.resize((stopped+1) - steps.begin());
		if ( move_recorder )
			move_recorder->add_movement(steps);
	}

	// Real return values are known now.
	if ( units_sighted_result != NULL )
		*units_sighted_result = !stops.seen_units.empty();
	if ( next_unit != NULL )
		*next_unit = final_loc;

	// If we stopped early because of sighted units, record the interrupted move.
	if(ui != units.end()) {
		if ( stops.sighted_stop && !stops.ambush_stop && !stops.event_mutated )
			ui->set_interrupted_move(route.back());
		else
			ui->set_interrupted_move(map_location());
	}

	// If we reached the end of the route, or were interrupted, clear the "goto" order.
	if ( ui != units.end() && (steps.size() == route.size() || stops.ambush_stop || stops.sighted_something || stops.event_mutated) ) {
		if ( move_spectator == NULL )
			ui->set_goto(map_location());
	}

	// Backup: in case we had to backtrack after being ambushed:
	if(!stops.event_stop) {
		foreach (const map_location &ambush, stops.ambushers) {
			const unit_map::iterator ambusher = units.find(ambush);
			if(units.end() != ambusher) {
				ambusher->set_state(unit::STATE_UNCOVERED, true);
			}
			disp.invalidate(ambush);
		}
	}

	// Show the final move animation step.
	disp.draw();

	// Process WML events.
	int orig_village_owner = -1;
	int action_time_bonus = 0;
	if(ui != units.end()) {
		stops.event_mutated |= movement_events(initial_loc, final_loc, orig_village_owner, action_time_bonus);
		//NOTE: an wml event may have removed the unit pointed by ui
		ui = units.find(final_loc);
	}
	disp.recalculate_minimap();
	disp.draw();

	// Record keeping.
	if ( move_spectator != NULL )
		move_spectator->set_unit(ui);

	if ( undo_stack != NULL ) {
		if ( stops.event_mutated || stops.block_undo || stops.display_changed || ui == units.end() ||
		    (resources::whiteboard->is_active() && resources::whiteboard->should_clear_undo()) )
		{
			apply_shroud_changes(*undo_stack, team_num + 1);
			undo_stack->clear();
		} else {
			// MP_COUNTDOWN: added param
			undo_stack->push_back(
				undo_action(*ui, steps, starting_moves,
						action_time_bonus, orig_village_owner, orig_dir));
		}
	}

	// On-screen messages/feedback.
	movement_feedback(stops, steps.size() < route.size(), tm, move_spectator);

	return steps.size();
}


bool unit_can_move(const unit &u)
{
	const team &current_team = (*resources::teams)[u.side() - 1];

	if(!u.attacks_left() && u.movement_left()==0)
		return false;

	// Units with goto commands that have already done their gotos this turn
	// (i.e. don't have full movement left) should have red globes.
	if(u.has_moved() && u.has_goto()) {
		return false;
	}

	map_location locs[6];
	get_adjacent_tiles(u.get_location(), locs);
	for(int n = 0; n != 6; ++n) {
		if (resources::game_map->on_board(locs[n])) {
			const unit_map::const_iterator i = resources::units->find(locs[n]);
			if (i.valid() && !i->incapacitated() &&
			    current_team.is_enemy(i->side())) {
				return true;
			}

			if (u.movement_cost((*resources::game_map)[locs[n]]) <= u.movement_left()) {
				return true;
			}
		}
	}

	return false;
}

void apply_shroud_changes(undo_list &undos, int side)
{
	team &tm = (*resources::teams)[side - 1];
	// No need to do this if the team isn't using fog or shroud.
	if (!tm.uses_shroud() && !tm.uses_fog())
		return;

	game_display &disp = *resources::screen;
	unit_map &units = *resources::units;

	/*
	   This function works thusly:
	   1. run through the list of undo_actions
	   2. for each one, play back the unit's move
	   3. for each location along the route, clear any "shrouded" hexes that the unit can see
	      and record sighted events
	   4. render shroud/fog cleared.
	   5. pump all events
	   6. call clear_shroud to update the fog of war for each unit
	   7. fix up associated display stuff (done in a similar way to turn_info::undo())
	*/

	std::set<map_location> known_units;
	foreach (const unit &u, units) {
		if ( u.side() == side  ||  !tm.fogged(u.get_location()) )
			known_units.insert(u.get_location());
	}

	bool cleared_shroud = false;  // for further optimization
	bool sighted_event = false;

	for(undo_list::iterator un = undos.begin(); un != undos.end(); ++un) {
		LOG_NG << "Turning an undo...\n";
		//NOTE: for the moment shroud cleared during recall seems never delayed
		//Shroud update during recall can be delayed, during recruit as well
		//if we have a non-random recruit (e.g. undead)
		//if(un->is_recall() || un->is_recruit()) continue;

		// Make a temporary unit move in map and hide the original
		const unit_map::const_unit_iterator unit_itor = units.find(un->affected_unit.underlying_id());
		// check if the unit is still existing (maybe killed by an event)
		// FIXME: A wml-killed unit will not update the shroud explored before its death
		if(unit_itor == units.end())
			continue;

		// Cache the unit's current actual location for raising the sighted events.
		const map_location actual_location = unit_itor->get_location();

		std::vector<map_location>::const_iterator step;
		for(step = un->route.begin(); step != un->route.end(); ++step) {
			// Clear the shroud, and collect new seen_units
			std::set<map_location> seen_units;
			std::set<map_location> petrified_units;
			std::map<map_location, int> jamming_map;
			calculate_jamming(side, jamming_map);
			cleared_shroud |= clear_shroud_unit(*step, *unit_itor, tm, jamming_map,
				&known_units,&seen_units,&petrified_units);

			// Fire sighted events
			// Try to keep same order (petrified units after normal units)
			// as with move_unit for replay
			for (std::set<map_location>::iterator sight_it = seen_units.begin();
				sight_it != seen_units.end(); ++sight_it)
			{
				unit_map::const_iterator new_unit = units.find(*sight_it);
				assert(new_unit != units.end());

				game_events::raise("sighted", *sight_it, actual_location);
				sighted_event = true;
			}
			for (std::set<map_location>::iterator sight_it = petrified_units.begin();
				sight_it != petrified_units.end(); ++sight_it)
			{
				unit_map::const_iterator new_unit = units.find(*sight_it);
				assert(new_unit != units.end());

				game_events::raise("sighted", *sight_it, actual_location);
				sighted_event = true;
			}
		}
	}

	// TODO: optimization: nothing cleared, so no sighted, and we can skip redraw
	// if (!cleared_shroud) return;

	// render shroud/fog cleared before pumping events
	if (sighted_event) {
		disp.invalidate_unit();
		disp.invalidate_all();
		disp.recalculate_minimap();
		disp.draw();
	}

	game_events::pump();

	// Update shroud (WML might have changed something) and invalidate stuff.
	disp.invalidate_unit();
	disp.invalidate_game_status();
	clear_shroud(side);
	disp.recalculate_minimap();
	disp.invalidate_all();
}

bool backstab_check(const map_location& attacker_loc,
		const map_location& defender_loc,
		const unit_map& units, const std::vector<team>& teams)
{
	const unit_map::const_iterator defender =
		units.find(defender_loc);
	if(defender == units.end()) return false; // No defender

	map_location adj[6];
	get_adjacent_tiles(defender_loc, adj);
	int i;
	for(i = 0; i != 6; ++i) {
		if(adj[i] == attacker_loc)
			break;
	}
	if(i >= 6) return false;  // Attack not from adjacent location

	const unit_map::const_iterator opp =
		units.find(adj[(i+3)%6]);
	if(opp == units.end()) return false; // No opposite unit
	if (opp->incapacitated()) return false;
	if (size_t(defender->side() - 1) >= teams.size() || size_t(opp->side() - 1) >= teams.size())
		return true; // If sides aren't valid teams, then they are enemies
	if (teams[defender->side() - 1].is_enemy(opp->side()))
		return true; // Defender and opposite are enemies
	return false; // Defender and opposite are friends
}
