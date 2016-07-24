#pragma once

#include "vision.hpp"
#include "map/location.hpp"
#include "units/ptr.hpp"

namespace actions
{
/// base class for classes that clear srhoud (move/recruit/recall)
struct shroud_clearing_action
{

	shroud_clearing_action(const config& cfg)
		: route()
		, view_info(cfg.child_or_empty("unit"))
		, original_village_owner(cfg["village_owner"].to_int())
		, take_village_timebonus(cfg["village_timebonus"].to_bool())
	{
		read_locations(cfg, route);
	}

	shroud_clearing_action(const unit_const_ptr u, const map_location& loc, int village_owner, bool village_bonus)
		: route(1, loc)
		, view_info(*u)
		, original_village_owner(village_owner)
		, take_village_timebonus(village_bonus)
	{

	}

	typedef std::vector<map_location> t_route;

	shroud_clearing_action(const unit_const_ptr u, const t_route::const_iterator& begin, const t_route::const_iterator& end, int village_owner, bool village_bonus)
		: route(begin, end)
		, view_info(*u)
		, original_village_owner(village_owner)
		, take_village_timebonus(village_bonus)
	{

	}

	/// The hexes occupied by the affected unit during this action.
	/// For recruits and recalls this only contains one hex.
	t_route route;
	/// A record of the affected unit's ability to see.
	clearer_info view_info;
	/// The number of the side that preivously owned the village that the unit stepped on
	/// Note, that recruit/recall actions can also take a village if the unit was recruits/recalled onto a village.
	int original_village_owner;
	/// Whether this actions got a timebonus becasue it took a village.
	bool take_village_timebonus;
	
	/// Change village owner on undo.
	void return_village();
	/// Change village owner on redo.
	void take_village();
	
	void write(config & cfg) const
	{
		write_locations(route, cfg);
		view_info.write(cfg.add_child("unit"));
		cfg["village_owner"] = original_village_owner;
		cfg["village_timebonus"] = take_village_timebonus;
	}

	virtual ~shroud_clearing_action() {}
};
}