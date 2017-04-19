/*
   Copyright (C) 2003 - 2017 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef UNIT_H_INCLUDED
#define UNIT_H_INCLUDED

#include "units/id.hpp"
#include "units/ptr.hpp"
#include "units/types.hpp"

#include <bitset>
#include <boost/dynamic_bitset_fwd.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/variant.hpp>

class display;
class display_context;
class gamemap;
class team;
class unit_animation_component;
class unit_formula_manager;
class vconfig;
struct color_t;

namespace unit_detail
{
	template<typename T>
	const T& get_or_default(const std::unique_ptr<T>& v)
	{
		if(v) {
			return *v;
		} else {
			static const T def;
			return def;
		}
	}
}

// Data typedef for unit_ability_list.
using unit_ability = std::pair<const config*, map_location>;

class unit_ability_list
{
public:
	unit_ability_list() : cfgs_() {}

	// Implemented in unit_abilities.cpp
	std::pair<int, map_location> highest(const std::string& key, int def=0) const;
	std::pair<int, map_location> lowest(const std::string& key, int def=0) const;

	// The following make this class usable with standard library algorithms and such
	typedef std::vector<unit_ability>::iterator       iterator;
	typedef std::vector<unit_ability>::const_iterator const_iterator;

	iterator       begin()        { return cfgs_.begin(); }
	const_iterator begin() const  { return cfgs_.begin(); }
	iterator       end()          { return cfgs_.end();   }
	const_iterator end()   const  { return cfgs_.end();   }

	// Vector access
	bool                empty() const  { return cfgs_.empty(); }
	unit_ability&       front()        { return cfgs_.front(); }
	const unit_ability& front() const  { return cfgs_.front(); }
	unit_ability&       back()         { return cfgs_.back();  }
	const unit_ability& back()  const  { return cfgs_.back();  }

	iterator erase(const iterator& erase_it)  { return cfgs_.erase(erase_it); }
	void push_back(const unit_ability& ability)  { cfgs_.push_back(ability); }

private:
	// Data
	std::vector<unit_ability> cfgs_;
};

/**
 * This class represents a *single* unit of a specific type.
 */
class unit
{
public:
	/**
	 * Clear this unit status cache for all units. Currently only the hidden
	 * status of units is cached this way.
	 */
	static void clear_status_caches();

	/** The path to the leader crown overlay. */
	static const std::string& leader_crown();

	/** Initializes a unit from a config */
	unit(const config& cfg, bool use_traits = false, const vconfig* vcfg = nullptr);

	/**
	 * Initializes a unit from a unit type.
	 *
	 * Only real_unit-s should have random traits, name and gender (to prevent OOS caused by RNG calls)
	 */
	unit(const unit_type& t, int side, bool real_unit, unit_race::GENDER gender = unit_race::NUM_GENDERS);

	// Copy constructor
	unit(const unit& u);

	virtual ~unit();

	void swap(unit&);

	unit& operator=(unit);

	/*** *** *** *** *** *** Advancement functions *** *** *** *** *** ***/

	/** Advances this unit to another type */
	void advance_to(const unit_type& t, bool use_traits = false);

	/**
	 * Gets the possible types this unit can advance to on level-up.
	 *
	 * @returns                   A list of type IDs this unit may advance to.
	 */
	const std::vector<std::string>& advances_to() const
	{
		return advances_to_;
	}

	/**
	 * Gets the names of the possible types this unit can advance to on level-up.
	 *
	 * @returns                   A list of the names of the types this unit may advance to.
	 */
	const std::vector<std::string> advances_to_translated() const;

	/**
	 * Sets this unit's advancement options.
	 *
	 * @param advances_to         A list of new type IDs this unit may advance to.
	 */
	void set_advances_to(const std::vector<std::string>& advances_to);

	/**
	 * Checks whether this unit has any options to advance to.
	 *
	 * This considers both whether it has types to advance to OR whether any modifications
	 * specify non-type advancement options.
	 *
	 * Note this does not consider unit experience at all, it only checks option availability.
	 * See @ref advances if an experience check is necessary.
	 */
	bool can_advance() const
	{
		return !advances_to_.empty() || !get_modification_advances().empty();
	}

	/**
	 * Checks whether this unit is eligible for level-up.
	 *
	 * @retval true              This unit has sufficient experience to level up and has advancement
	 *                           options available.
	 */
	bool advances() const
	{
		return experience_ >= max_experience() && can_advance();
	}

	/**
	 * Gets and image path and and associated description for each advancement option.
	 *
	 * Covers both type and modification-based advancements.
	 *
	 * @returns                  A data map, in image/description format. If the option is a unit type,
	 *                           advancement, the key is the type's image and the value the type ID.
	 *
	 *                           If the option is a modification, the key and value are set from config data
	 *                           (see @ref get_modification_advances).
	 */
	std::map<std::string, std::string> advancement_icons() const;

	/**
	 * Gets any non-typed advanced options set by modifications.
	 *
	 * These are usually used to give a unit special advancement options that don't invole transforming to a
	 * new type.
	 *
	 * Note this is not the raw option data. Parsing is performed to ensure each option appears only once.
	 * Use @ref modification_advancements is the raw data is needed.
	 *
	 * @returns                  A config list of options data. Each option is unique.
	 */
	std::vector<config> get_modification_advances() const;

	/**
	 * Gets the image and description data for modification advancements.
	 *
	 * @returns                  A list of pairs of the image paths(first) and descriptions (second) for
	 *                           each advancement option.
	 */
	std::vector<std::pair<std::string, std::string>> amla_icons() const;

	/** The raw, unparsed data for modification advancements. */
	using advancements_list= boost::ptr_vector<config>;
	const advancements_list& modification_advancements() const
	{
		return advancements_;
	}

	/** Sets the raw modification advancement option data */
	void set_advancements(std::vector<config> advancements);

	/*** *** *** *** *** *** Basic data setters and getters *** *** *** *** *** ***/

public:
	/**
	 * The side this unit belongs to.
	 *
	 * Note that side numbers starts from 1, not 0, so be sure to subtract 1 if using as a container index.
	 */
	int side() const
	{
		return side_;
	}

	/** Sets the side this unit belongs to. */
	void set_side(unsigned int new_side)
	{
		side_ = new_side;
	}

	/** This unit's type, accounting for gender and variation. */
	const unit_type& type() const
	{
		return *type_;
	}

	/**
	 * The id of this unit's type.
	 *
	 * If you are dealing with creating units (e.g. recruitment), this is not what you want, as a
	 * variation can change this; use type().base_id() instead.
	 */
	const std::string& type_id() const
	{
		return type_->id();
	}

	/** Gets the translatable name of this unit's type. */
	const t_string& type_name() const
	{
		return type_name_;
	}

	/**
	 * Gets this unit's id.
	 *
	 * This is a unique string usually set by WML. It should *not* be used for internal tracking in
	 * the unit_map. Use @ref underlying_id for that.
	 *
	 * @param fallback           If true (default), return this unit's type name if the id is empty.
	 */
	const std::string& id(bool fallback = true) const
	{
		if(fallback && id_.empty()) {
			return type_name();
		}

		return id_;
	}

	/** Sets this unit's string ID. */
	void set_id(const std::string& id)
	{
		id_ = id;
	}

	DEPRECATED("Use comparison against id() instead")
	bool matches_id(const std::string& unit_id) const
	{
		return id_ == unit_id;
	}

	/** This unit's unique internal ID. This should *not* be used for user-facing operations. */
	size_t underlying_id() const
	{
		return underlying_id_.value;
	}

private:
	/** Sets the internal ID. */
	void set_underlying_id(n_unit::id_manager& id_manager);

public:
	/** Gets this unit's translatable display name. */
	const t_string& name() const
	{
		return name_;
	}

	/**
	 * Sets this unit's translatable display name.
	 *
	 * This should only be used internally since it ignores the 'unrenamable' flag.
	 */
	void set_name(const t_string& name)
	{
		name_ = name;
	}

	/**
	 * Attemps to this unit's translatable display name, taking the 'unrenamable' flag into account.
	 *
	 * If a direct rename is desired, use @ref set_name.
	 * @todo should this also take a t_string?
	 */
	void rename(const std::string& name)
	{
		if(!unrenamable_) {
			name_ = name;
		}
	}

	/**
	 * Whether this unit can be renamed.
	 *
	 * This flag is considered by @ref rename, but not @ref set_name.
	 */
	bool unrenamable() const
	{
		return unrenamable_;
	}

	/**
	 * Sets the 'unrenamable' flag. Usually used for scenario-specific units which should not be renamed.
	 */
	void set_unrenamable(bool unrenamable)
	{
		unrenamable_ = unrenamable;
	}

	/** A detailed description of this unit. */
	t_string unit_description() const
	{
		return description_;
	}

	/** The gender of this unit. */
	unit_race::GENDER gender() const
	{
		return gender_;
	}

	/**
	 * The alignment of this unit.
	 *
	 * This affects the time of day during which this unit's attacks do the most damage.
	 */
	unit_type::ALIGNMENT alignment() const
	{
		return alignment_;
	}

	/** Sets the alignment of this unit. */
	void set_alignment(unit_type::ALIGNMENT alignment)
	{
		alignment_ = alignment;
	}

	/**
	 * Gets this unit's race.
	 *
	 * @returns                  A pointer to a unit_race object - never nullptr, but it may point
	 *                           to the null race.
	 */
	const unit_race* race() const
	{
		return race_;
	}

	/** The current number of hitpoints this unit has. */
	int hitpoints() const
	{
		return hit_points_;
	}

	/** The max number of hitpoints this unit can have. */
	int max_hitpoints() const
	{
		return max_hit_points_;
	}

	/** Sets the current hitpoint amount. */
	void set_hitpoints(int hp)
	{
		hit_points_ = hp;
	}

	/** The current number of experience points this unit has. */
	int experience() const
	{
		return experience_;
	}

	/** The max number of experience points this unit can have. */
	int max_experience() const
	{
		return max_experience_;
	}

	/** Sets the current experience point amount. */
	void set_experience(int xp)
	{
		experience_ = xp;
	}

	/** The current level of this unit. */
	int level() const
	{
		return level_;
	}

	/** Sets the current level of this unit. */
	void set_level(int level)
	{
		level_ = level;
	}

	/** The ID of the variation of this unit's type. */
	const std::string& variation() const
	{
		return variation_;
	}

	/** The ID of the undead variation (ie, dwarf, swimmer) of this unit. */
	const std::string& undead_variation() const
	{
		return undead_variation_;
	}

	/**
	 * An optional profile image to display in Help.
	 *
	 * @returns                   The specified image, this unit's type's sprite image if empty
	 *                            or 'unit_image' was set.
	 */
	std::string small_profile() const;

	/**
	 * An optional profile image displays when this unit is 'speaking' via [message].
	 *
	 * @returns                   The specified image, this unit's type's sprite image if empty
	 *                            or 'unit_image' was set.
	 */
	std::string big_profile() const;

	/** Whether this unit can recruit other units - ie, are they a leader unit. */
	bool can_recruit() const
	{
		return canrecruit_;
	}

	/** Sets whether this unit can recruit other units. */
	void set_can_recruit(bool canrecruit)
	{
		canrecruit_ = canrecruit;
	}

	/** The type IDs of the other units this unit may recruit, if possible. */
	const std::vector<std::string>& recruits() const
	{
		return recruit_list_;
	}

	/** Sets the recruit list. */
	void set_recruits(const std::vector<std::string>& recruits);

	/** How much gold is required to recruit this unit. */
	int cost() const
	{
		return unit_value_;
	}

	/** How much gold it costs to recall this unit. */
	int recall_cost() const
	{
		return recall_cost_;
	}

	/** Sets the cost of recalling this unit. */
	void set_recall_cost(int recall_cost)
	{
		recall_cost_ = recall_cost;
	}

	/** Gets the filter contraints upon which units this unit may recall, if able. */
	const config& recall_filter() const
	{
		return filter_recall_;
	}

	/**
	 * Gets this unit's role.
	 *
	 * A role is a special string flag usually used to represent a unit's purpose in a scenario.
	 * It can be filtered on.
	 */
	const std::string& get_role() const
	{
		return role_;
	}

	/** Sets a unit's role */
	void set_role(const std::string& role)
	{
		role_ = role;
	}

	/**
	 * Gets this unit's usage. This is relevant to the AI.
	 *
	 * Usage refers to how the AI may consider utilizing this unit in combat.
	 * @todo document further
	 */
	std::string usage() const
	{
		return unit_detail::get_or_default(usage_);
	}

	/** Sets this unit's usage. */
	void set_usage(const std::string& usage)
	{
		usage_.reset(new std::string(usage));
	}

	/**
	 * Gets any user-defined variables this unit 'owns'.
	 *
	 * These are accessible via WML if the unit's data is serialized to a variable. They're strictly
	 * user-facing; internal engine calculations shouldn't use this.
	 */
	config& variables()
	{
		return variables_;
	}

	/** Const overload of @ref variables. */
	const config& variables() const
	{
		return variables_;
	}

	/**
	 * Gets whether this unit is currently hidden on the map.
	 * @todo document hiddenness
	 */
	bool get_hidden() const
	{
		return hidden_;
	}

	/** Sets whether the unit is hidden on the map. */
	void set_hidden(bool state) const;

	/**
	 * The factor by which the HP bar should be scaled.
	 * @todo: document further
	 */
	double hp_bar_scaling() const
	{
		return hp_bar_scaling_;
	}

	/**
	 * The factor by which the XP bar should be scaled.
	 * @todo: document further
	 */
	double xp_bar_scaling() const
	{
		return xp_bar_scaling_;
	}

	bool hold_position() const
	{
		return hold_position_;
	}

	void toggle_hold_position()
	{
		hold_position_ = !hold_position_;
		if(hold_position_) {
			end_turn_ = true;
		}
	}

	void set_user_end_turn(bool value = true)
	{
		end_turn_ = value;
	}

	void toggle_user_end_turn()
	{
		end_turn_ = !end_turn_;
		if(!end_turn_) {
			hold_position_ = false;
		}
	}

	bool user_end_turn() const
	{
		return end_turn_;
	}

	void new_turn();
	void end_turn();
	void new_scenario();

	bool take_hit(int damage)
	{
		hit_points_ -= damage;
		return hit_points_ <= 0;
	}

	void heal(int amount);
	void heal_fully()
	{
		hit_points_ = max_hitpoints();
	}

	const std::set<std::string> get_states() const;
	bool get_state(const std::string& state) const;
	void set_state(const std::string& state, bool value);

	enum state_t {
		STATE_SLOWED = 0,
		STATE_POISONED,
		STATE_PETRIFIED,
		STATE_UNCOVERED,
		STATE_NOT_MOVED,
		STATE_UNHEALABLE,
		STATE_GUARDIAN,
		STATE_UNKNOWN = -1
	};

	void set_state(state_t state, bool value);
	bool get_state(state_t state) const;

	static state_t get_known_boolean_state_id(const std::string& state);

	bool poisoned() const
	{
		return get_state(STATE_POISONED);
	}

	bool incapacitated() const
	{
		return get_state(STATE_PETRIFIED);
	}

	bool slowed() const
	{
		return get_state(STATE_SLOWED);
	}

	/*** *** *** *** *** *** Attack and resistance functions. *** *** *** *** *** ***/

public:
	/** Gets an iterator over this unit's attacks. */
	attack_itors attacks()
	{
		return make_attack_itors(attacks_);
	}

	/** Const overload of @ref attacks. */
	const_attack_itors attacks() const
	{
		return make_attack_itors(attacks_);
	}

	template<typename... Args>
	attack_ptr add_attack(attack_itors::iterator position, Args... args)
	{
		return *attacks_.emplace(position.base(), new attack_type(args...));
	}

	bool remove_attack(attack_ptr atk);
	void remove_attacks_ai();

	/**
	 * Calculates the damage this unit would take from a certain attack.
	 *
	 * @param attack              The attack to consider.
	 * @param attacker            Whether this unit should be considered the attacker.
	 * @param loc                 TODO: what does this do?
	 *
	 * @returns                   The expected damage.
	 */
	int damage_from(const attack_type& attack, bool attacker, const map_location& loc) const
	{
		return resistance_against(attack, attacker, loc);
	}

	/** The maximum number of attacks this unit may perform per turn, usually 1. */
	int max_attacks() const
	{
		return max_attacks_;
	}

	/**
	 * Gets the remaining number of attacks this unit can perform this turn.
	 *
	 * If the 'incapacitated' is set, this will always be 0.
	 */
	int attacks_left() const
	{
		return (attacks_left_ == 0 || incapacitated()) ? 0 : attacks_left_;
	}

	/** Sets the number of attacks this unit has left this turn. */
	void set_attacks(int left)
	{
		attacks_left_ = std::max<int>(0, left);
	}

	int defense_modifier(const t_translation::terrain_code& terrain) const;

	int resistance_against(const std::string& damage_name, bool attacker, const map_location& loc) const;
	int resistance_against(const attack_type& damage_type, bool attacker, const map_location& loc) const
	{
		return resistance_against(damage_type.type(), attacker, loc);
	}

	/** Gets resistances without any abilities applied. */
	utils::string_map get_base_resistances() const
	{
		return movement_type_.damage_table();
	}

private:
	bool resistance_filter_matches(const config& cfg, bool attacker, const std::string& damage_name, int res) const;

	/*** *** *** *** *** *** Trait and upkeep functions. *** *** *** *** *** ***/
public:
	/**
	 * Applies mandatory traits (e.g. undead, mechanical) to a unit and then fills in the remaining traits
	 * traits until no more are available (leaders have a restricted set of available traits) or the unit has
	 * its maximum number of traits.
	 *
	 * This routine does not apply the effects of added traits to a unit; that must be done by the caller.
	 *
	 * Note that random numbers used in config files don't work in multiplayer, so leaders should be barred
	 * from all random traits until that is fixed. Later the restrictions will be based on play balance.
	 *
	 * @param must_have_only      Whether random or optional traits should be included or not. If false only
	 *                            mandatory traits will be used.
	 */
	void generate_traits(bool must_have_only = false);

	/**
	 * Gets the names of the currently registered traits.
	 *
	 * @returns                   A list of translatable trait names.
	 */
	const std::vector<t_string>& trait_names() const
	{
		return trait_names_;
	}

	/**
	 * Gets the descriptions of the currently registered traits.
	 *
	 * @returns                   A list of translatable trait descriptions.
	 */
	const std::vector<t_string>& trait_descriptions() const
	{
		return trait_descriptions_;
	}

	/**
	 * Gets a list of the traits this unit currently has.
	 *
	 * @returns                   A list of trait IDs.
	 */
	std::vector<std::string> get_traits_list() const;

	/**
	 * Register a trait's name and its description for the UI's use.
	 *
	 * The resulting data can be fetched with @ref trait_names and @ref trait_descriptions.
	 *
	 * @param trait               A config containing the trait's attributes.
	 * @param description         The translatable description of the trait.
	 */
	void add_trait_description(const config& trait, const t_string& description);

	/**
	 * Gets the amount of gold this unit costs a side per turn.
	 *
	 * This fetches an actual numeric gold value:
	 * - If @rec can_recruit is true, no upkeep is paid (0 is returned).
	 * - If a special upkeep flag is set, the associated gold amount is returned (see @ref upkeep_value_visitor).
	 * - If a numeric value is already set, it is returned directly.
	 *
	 * @returns                   A gold value, evaluated based on the state of @ref upkeep_raw.
	 */
	int upkeep() const;

	struct upkeep_full
	{
		static std::string type() { static std::string v = "full"; return v; }
	};

	struct upkeep_loyal
	{
		static std::string type() { static std::string v = "loyal"; return v; }
	};

	/** Visitor helper class to fetch the appropriate upkeep value. */
	class upkeep_value_visitor : public boost::static_visitor<int>
	{
	public:
		explicit upkeep_value_visitor(const unit& unit) : u_(unit) {}

		/** Full upkeep equals the unit's level. */
		int operator()(const upkeep_full&) const
		{
			return u_.level();
		}

		/** Loyal units cost no upkeep. */
		int operator()(const upkeep_loyal&) const
		{
			return 0;
		}

		int operator()(int v) const
		{
			return v;
		}

	private:
		const unit& u_;
	};

	/** Visitor helper struct to fetch the upkeep type flag if applicable, or the the value otherwise. */
	struct upkeep_type_visitor : public boost::static_visitor<std::string>
	{
		template<typename T>
		typename std::enable_if<!std::is_same<int, T>::value, std::string>::type
		operator()(T& v) const
		{
			// Any special upkeep type should have an associated @ref type getter in its helper struct.
			return v.type();
		}

		std::string operator()(int v) const
		{
			return std::to_string(v);
		}
	};

	using upkeep_t = boost::variant<upkeep_full, upkeep_loyal, int>;

	/**
	 * Gets the raw variant controlling the upkeep value.
	 *
	 * This should not usually be called directly. To get an actual numeric value of upkeep use @ref upkeep.
	 */
	upkeep_t upkeep_raw() const
	{
		return upkeep_;
	}

	/** Sets the upkeep value to a specific value value. Does not necessarily need to be numeric */
	void set_upkeep(upkeep_t v)
	{
		upkeep_ = v;
	}

	/** Gets whether this unit is loyal - ie, it costs no upkeep. */
	bool loyal() const;

	bool is_fearless() const
	{
		return is_fearless_;
	}

	bool is_healthy() const
	{
		return is_healthy_;
	}

	/*** *** *** *** *** *** Movement and location functions. *** *** *** *** *** ***/

public:
	/** The maximum moves this unit has. */
	int total_movement() const
	{
		return max_movement_;
	}

	/**
	 * Gets how far a unit can move, considering the `incapacitated` flag.
	 *
	 * @returns                   The remaining movement, or zero if incapacitated.
	 */
	int movement_left() const
	{
		return (movement_ == 0 || incapacitated()) ? 0 : movement_;
	}

	/**
	 * Gets how far a unit can move.
	 *
	 * @param base_value          If true, consider the `incapacitated` flag.
	 *
	 * @returns                   If @a base_value is false, the raw value is returned.
	 */
	int movement_left(bool base_value) const
	{
		return base_value ? movement_ : movement_left();
	}

	/**
	 * Set this unit's remaining movement to @a moves.
	 *
	 * This does not affect maximum movement.
	 *
	 * @param moves               The new number of moves
	 * @param unit_action         If to true, the "end turn" and "hold position" flags will be cleared
	 *                            (as they should be if a unit acts, as opposed to the movement being set
	 *                            by the engine for other reasons).
	 */
	void set_movement(int moves, bool unit_action = false);

	/** Checks if this unit has moved. */
	bool has_moved() const
	{
		return movement_left() != total_movement();
	}

	void remove_movement_ai();

	/**
	 * Checks whether this unit is 'resting'.
	 *
	 * Resting refers to whether this unit has not moved yet this turn. Note that this can be true even
	 * if @ref movement_left is not equal to @ref total_movement.
	 */
	bool resting() const
	{
		return resting_;
	}

	/** Sets this unit's resting status. */
	void set_resting(bool rest)
	{
		resting_ = rest;
	}

	/** Tests whether the unit has a zone-of-control, considering @ref incapacitated. */
	bool emits_zoc() const
	{
		return emit_zoc_  && !incapacitated();
	}

	/** Gets the raw zone-of-control flag, disregarding @ref incapacitated. */
	bool get_emit_zoc() const
	{
		return emit_zoc_;
	}

	/** Sets the raw zone-of-control flag. */
	void set_emit_zoc(bool val)
	{
		emit_zoc_ = val;
	}

	/** The current map location this unit is at. */
	const map_location& get_location() const
	{
		return loc_;
	}

	/**
	 * Sets this unit's map location.
	 *
	 * Note this should only be called by unit_map or for temporary units.
	 */
	void set_location(const map_location& loc)
	{
		loc_ = loc;
	}

	/** The current directin this unit is facing within its hex. */
	map_location::DIRECTION facing() const
	{
		return facing_;
	}

	/** The this unit's facing. */
	void set_facing(map_location::DIRECTION dir) const;

	/** Gets whether this unit has a multi-turn destination set. */
	bool has_goto() const
	{
		return get_goto().valid();
	}

	/** The map location to which this unit is moving over multiple turns, if any. */
	const map_location& get_goto() const
	{
		return goto_;
	}

	/** Sets this unit's long term destination. */
	void set_goto(const map_location& new_goto)
	{
		goto_ = new_goto;
	}

	int vision() const
	{
		return vision_ < 0 ? max_movement_ : vision_;
	}

	int jamming() const
	{
		return jamming_;
	}

	bool move_interrupted() const
	{
		return movement_left() > 0 && interrupted_move_.x >= 0 && interrupted_move_.y >= 0;
	}

	const map_location& get_interrupted_move() const
	{
		return interrupted_move_;
	}

	void set_interrupted_move(const map_location& interrupted_move)
	{
		interrupted_move_ = interrupted_move;
	}

	const movetype& movement_type() const
	{
		return movement_type_;
	}

	int movement_cost(const t_translation::terrain_code& terrain) const
	{
		return movement_type_.movement_cost(terrain, get_state(STATE_SLOWED));
	}

	int vision_cost(const t_translation::terrain_code& terrain) const
	{
		return movement_type_.vision_cost(terrain, get_state(STATE_SLOWED));
	}

	int jamming_cost(const t_translation::terrain_code& terrain) const
	{
		return movement_type_.jamming_cost(terrain, get_state(STATE_SLOWED));
	}

	bool is_flying() const
	{
		return movement_type_.is_flying();
	}

	/***** ***** ***** ***** Modification functions. ***** ***** ***** *****/

public:
	config& get_modifications()
	{
		return modifications_;
	}

	const config& get_modifications() const
	{
		return modifications_;
	}

	size_t modification_count(const std::string& type, const std::string& id) const;

	void add_modification(const std::string& type, const config& modification, bool no_add = false);

	/**
	 * Clears those modifications whose duration has expired.
	 *
	 * @param duration            If empty, all temporary modifications (those not lasting forever) expire.
	 *                            Otherwise, modifications whose duration equals @a duration expire.
	 */
	void expire_modifications(const std::string& duration);

	static const std::set<std::string> builtin_effects;

	void apply_builtin_effect(std::string type, const config& effect);

	std::string describe_builtin_effect(std::string type, const config& effect);

	void apply_modifications();

	/***** ***** ***** ***** Image and animations functions. ***** ***** ***** *****/

public:
	unit_animation_component& anim_comp() const
	{
		return *anim_comp_;
	}

	/** The name of the file to game_display (used in menus). */
	std::string absolute_image() const;

	/** The default image to use for animation frames with no defined image. */
	std::string default_anim_image() const;

	std::string image_halo() const
	{
		return unit_detail::get_or_default(halo_);
	}

	void set_image_halo(const std::string& halo);

	std::string image_ellipse() const
	{
		return unit_detail::get_or_default(ellipse_);
	}

	void set_image_ellipse(const std::string& ellipse)
	{
		ellipse_.reset(new std::string(ellipse));
	}

	/**
	 * Get the source color palette to use when recoloring the unit's image.
	 */
	const std::string& flag_rgb() const;

	/** Constructs a recolor (RC) IPF string for this unit's team color. */
	std::string TC_image_mods() const;

	/** Gets any IPF image mods applied by effects. */
	const std::string& effect_image_mods() const
	{
		return image_mods_;
	}

	/**
	 * Gets an IPF string containing all IPF image mods.
	 *
	 * @returns                 An amalgamation of @ref effect_image_mods followed by @ref TC_image_mods.
	 */
	std::string image_mods() const;

	const std::vector<std::string>& overlays() const
	{
		return overlays_;
	}

	/**
	 * Color for this unit's *current* hitpoints.
	 *
	 * @returns                   A color between green and red representing how wounded this unit is.
	 *                            The maximum_hitpoints are considered as base.
	 */
	color_t hp_color() const;

	/**
	 * Color for this unit's hitpoints.
	 *
	 * @param hitpoints           The number of hitpoints the color represents.
	 * @returns                   The color considering the current hitpoints as base.
	 */
	color_t hp_color(int hitpoints) const;

	/**
	 * Color for this unit's XP. See also @ref hp_color
	 */
	color_t xp_color() const;

	/***** ***** ***** ***** Ability functions. ***** ***** ***** *****/

public:
	/**
	 * Checks whether this unit currently possesses or is affected by a given ability.
	 *
	 * This means that the ability could be owned by this unit itself or by an adjacent unit, should
	 * the ability affect an AoE in which this unit happens to be.
	 *
	 * @param tag_name            The name of the ability to check for.
	 * @param loc                 The location around which to check for affected units. This may or
	 *                            may not be the location of this unit.
	 * @param dc                  A display_context object, used in calculations.
	 */
	bool get_ability_bool(const std::string& tag_name, const map_location& loc, const display_context& dc) const;

	/**
	 * Checks whether this unit currently possesses or is affected by a given ability.
	 *
	 * This means that the ability could be owned by this unit itself or by an adjacent unit, should
	 * the ability affect an AoE in which this unit happens to be.
	 *
	 * This overload uses the location of this unit for calculations.
	 *
	 * @param tag_name            The name of the ability to check for.
	 * @param dc                  A display_context object, used in calculations.
	 */
	bool get_ability_bool(const std::string& tag_name, const display_context& dc) const
	{
		return get_ability_bool(tag_name, loc_, dc);
	}

	unit_ability_list get_abilities(const std::string& tag_name, const map_location& loc) const;
	unit_ability_list get_abilities(const std::string& tag_name) const
	{
		return get_abilities(tag_name, loc_);
	}

	/**
	 * Gets the names and descriptions of this unit's abilities.
	 *
	 * @param active_list         If nullptr, then all abilities are forced active. If not, this vector
	 *                            will be the same length as the returned one and will indicate whether or
	 *                            not the corresponding ability is active.
	 *
	 * @returns                   A list of triples consisting of (in order) base name, male or female name as
	 *                            appropriate for the unit, and description.
	 */
	std::vector<std::tuple<t_string, t_string, t_string>>
	ability_tooltips(boost::dynamic_bitset<>* active_list = nullptr) const;

	std::vector<std::string> get_ability_list() const;

	bool has_ability_type(const std::string& ability) const;

	// Needed for unit_filter
	bool has_ability_by_id(const std::string& ability) const;

	void remove_ability_by_id(const std::string& ability);

private:
	/**
	 * @param cfg: an ability WML structure
	 */
	bool ability_active(const std::string& ability, const config& cfg, const map_location& loc) const;
	bool ability_affects_adjacent(const std::string& ability, const config& cfg, int dir, const map_location& loc, const unit& from) const;
	bool ability_affects_self(const std::string& ability, const config& cfg, const map_location& loc) const;

public:
	unit_formula_manager& formula_manager() const
	{
		return *formula_man_;
	}

	/** Generates a random race-appropriate name if one has not already been provided. */
	void generate_name();

	// Only see_all = true use caching
	bool invisible(const map_location& loc, const display_context& dc, bool see_all = true) const;

	bool is_visible_to_team(const team& team, const display_context& dc, bool const see_all = true) const;

	/**
	 * Serializes the current unit metadata values.
	 *
	 * @param cfg                 The config to write to.
	 */
	void write(config& cfg) const;

	/**
	 * Mark this unit as clone so it can be inserted to unit_map.
	 *
	 * @returns                   self (for convenience)
	 */
	unit& clone(bool is_temporary = true);

	long ref_count() const
	{
		return ref_count_;
	}

	friend void intrusive_ptr_add_ref(const unit*);
	friend void intrusive_ptr_release(const unit*);

protected:
	mutable long ref_count_; // used by intrusive_ptr

private:
	map_location loc_;

	std::vector<std::string> advances_to_;

	/** Never nullptr. Adjusted for gender and variation. */
	const unit_type* type_;

	/** The displayed name of this unit type. */
	t_string type_name_;

	/** Never nullptr, but may point to the null race. */
	const unit_race* race_;

	std::string id_;
	t_string name_;
	n_unit::unit_id underlying_id_;

	std::string undead_variation_;
	std::string variation_;

	int hit_points_;
	int max_hit_points_;
	int experience_;
	int max_experience_;

	int level_;

	int recall_cost_;
	bool canrecruit_;
	std::vector<std::string> recruit_list_;
	unit_type::ALIGNMENT alignment_;

	std::string flag_rgb_;
	std::string image_mods_;

	bool unrenamable_;

	int side_;

	unit_race::GENDER gender_;

	std::unique_ptr<unit_formula_manager> formula_man_;

	int movement_;
	int max_movement_;
	int vision_;
	int jamming_;

	movetype movement_type_;

	bool hold_position_;
	bool end_turn_;
	bool resting_;

	int attacks_left_;
	int max_attacks_;

	std::set<std::string> states_;

	static const size_t num_bool_states = 7;

	std::bitset<num_bool_states> known_boolean_states_;
	static std::map<std::string, state_t> known_boolean_state_names_;

	config variables_;
	config events_;
	config filter_recall_;

	bool emit_zoc_;

	std::vector<std::string> overlays_;

	std::string role_;
	attack_list attacks_;

protected:
	// TODO: I think we actually consider this to be part of the gamestate, so it might be better if it's not mutable,
	// but it's not easy to separate this guy from the animation code right now.
	mutable map_location::DIRECTION facing_;

private:
	std::vector<t_string> trait_names_;
	std::vector<t_string> trait_descriptions_;

	int unit_value_;
	map_location goto_, interrupted_move_;

	bool is_fearless_, is_healthy_;

	utils::string_map modification_descriptions_;

	// Animations:
	friend class unit_animation_component;

	std::unique_ptr<unit_animation_component> anim_comp_;

	bool getsHit_;
	mutable bool hidden_;
	double hp_bar_scaling_, xp_bar_scaling_;

	config modifications_;
	config abilities_;

	advancements_list advancements_;

	t_string description_;

	std::unique_ptr<std::string> usage_;
	std::unique_ptr<std::string> halo_;
	std::unique_ptr<std::string> ellipse_;

	bool random_traits_;
	bool generate_name_;

	upkeep_t upkeep_;

	std::string profile_;
	std::string small_profile_;

	void parse_upkeep(const config::attribute_value& upkeep);
	void write_upkeep(config::attribute_value& upkeep) const;

	/**
	 * Hold the visibility status cache for a unit, when not uncovered.
	 * This is mutable since it is a cache.
	 */
	mutable std::map<map_location, bool> invisibility_cache_;

	/**
	 * Clears the cache.
	 *
	 * Since we don't change the state of the object we're marked const (also
	 * required since the objects in the cache need to be marked const).
	 */
	void clear_visibility_cache() const
	{
		invisibility_cache_.clear();
	}
};

/**
 * Object which temporarily resets a unit's movement.
 *
 * @warning A unit whose movement is reset may not be deleted while held in a
 * @ref unit_movement_resetter object, so it's best to use thus only in a small scope.
 */
struct unit_movement_resetter
{
	unit_movement_resetter(const unit_movement_resetter&) = delete;
	unit_movement_resetter& operator=(const unit_movement_resetter&) = delete;

	unit_movement_resetter(const unit& u, bool operate = true);
	~unit_movement_resetter();

private:
	unit& u_;
	int moves_;
};

/**
 * Gets a checksum for a unit.
 *
 * In MP games the descriptions are locally generated and might differ, so it
 * should be possible to discard them.  Not sure whether replays suffer the
 * same problem.
 *
 *  @param u                    this unit
 *
 *  @returns                    the checksum for a unit
 */
std::string get_checksum(const unit& u);

#endif
