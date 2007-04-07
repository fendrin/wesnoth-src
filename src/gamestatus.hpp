/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef GAME_STATUS_HPP_INCLUDED
#define GAME_STATUS_HPP_INCLUDED

#include "unit.hpp"

#include <time.h>

#include <string>
#include <vector>

//an object which defines the current time of day
struct time_of_day
{
	explicit time_of_day(const config& cfg);
	void write(config& cfg) const;

	//the % bonus lawful units receive. chaotic units will
	//receive -lawful_bonus.
	int lawful_bonus;
	int bonus_modified;

	//the image to be displayed in the game status.
	std::string image;
	t_string name;
	std::string id;

	//the image that is to be laid over all images while it's this
	//time of day
	std::string image_mask;

	//the colour modifications that should
	//be made to the game board to reflect the time of day.
	int red, green, blue;
};

/** Information on a particular player of the game. */
struct player_info
{
	player_info():gold(-1) {}

	std::string name; /** < to sore the current_player name */
	int gold; /** < amount of gold the player has saved */
	std::vector<unit> available_units; /** < units the player may recall */

	std::set<std::string> can_recruit; /** < units the player has the ability to recruit */
};

struct game_state : public variable_set
{
	game_state() : difficulty("NORMAL") {}
	std::string label; //name of the game (e.g. name of save file)
	std::string version; //version game was created with.
	std::string campaign_type; //type of the game - campaign, multiplayer etc

	std::string campaign_define; //if there is a define the campaign uses to customize data

	std::string campaign; //the campaign being played
	std::string scenario; //the scenario being played

	// information about campaign players who carry resources from
	// previous levels, indexed by a string identifier (which is
	// the leader name by default, but can be set with the "id"
	// attribute of the "side" tag)
	std::map<std::string, player_info> players;

	// Return the Nth player, or NULL if no such player exists
	player_info* get_player(const std::string& id);

	config variables; //variables that have been set

	//Variable access
	t_string& get_variable(const std::string& varname);
	virtual const t_string& get_variable_const(const std::string& varname);

	config& get_variable_cfg(const std::string& varname);
	void set_variable(const std::string& varname, const t_string& value);

	void clear_variable(const std::string& varname);

	std::string difficulty; //the difficulty level the game is being played on.

	//if the game is saved mid-level, we have a series of replay steps to
	//take the game up to the position it was saved at.
	config replay_data;

	//for multiplayer games, the position the game started in may be different to
	//the scenario, so we save the starting state of the game here.
	config starting_pos;

	//the snapshot of the game's current contents. i.e. unless the player selects
	//to view a replay, the game's settings are read in from this object
	config snapshot;

private:
	void get_variable_internal(const std::string& key, config& cfg,
			t_string** varout, config** cfgout);
};

//class which contains the global status of the game -- namely
//the current turn, the number of turns, and the time of day.
class gamestatus
{
public:
        gamestatus(const config& time_cfg, int num_turns, game_state* s_o_g = 0);
	void write(config& cfg) const;

	time_of_day get_time_of_day() const;
	time_of_day get_previous_time_of_day() const;
	time_of_day get_time_of_day(int illuminated, const gamemap::location& loc) const;
	time_of_day get_time_of_day(int illuminated, const gamemap::location& loc, int n_turn) const;
	size_t turn() const;
	int number_of_turns() const;
	void modify_turns(const std::string& mod);
	void add_turns(int num);

	//function to move to the next turn. Returns true iff time
	//has expired.
	bool next_turn();

private:
	time_of_day get_time_of_day_turn(int nturn) const;

	std::vector<time_of_day> times_;

	struct area_time_of_day {
		std::string xsrc, ysrc;
		std::vector<time_of_day> times;
		std::set<gamemap::location> hexes;
	};

	std::vector<area_time_of_day> areas_;

	size_t turn_;
	int numTurns_;
};

//object which holds all the data needed to start a scenario.
//i.e. this is the object serialized to disk when saving/loading a game.
//is also the object which needs to be created to start a new game

struct save_info {
	save_info(const std::string& n, time_t t) : name(n), time_modified(t) {}
	std::string name;
	time_t time_modified;
};

//function to get a list of available saves.
std::vector<save_info> get_saves_list(const std::string *dir = NULL);

enum WRITE_GAME_MODE { WRITE_SNAPSHOT_ONLY, WRITE_FULL_GAME };

void read_save_file(const std::string& name, config& cfg, std::string* error_log);

game_state read_game(const game_data& data, const config* cfg);
void write_game(const game_state& game, config& cfg, WRITE_GAME_MODE mode=WRITE_FULL_GAME);

// function returns true iff there is already savegame with that name
bool save_game_exists(const std::string & name);

//functions to load/save games.
void load_game(const game_data& data, const std::string& name, game_state& state, std::string* error_log);
void load_game_summary(const std::string& name, config& cfg_summary, std::string* error_log);
//throws gamestatus::save_game_failed
void save_game(const game_state& state);

//function to delete a save
void delete_game(const std::string& name);

config& save_summary(const std::string& save);

void write_save_index();

#endif
