/* $Id$ */
/*
   Copyright (C) 2008 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef EDITOR2_MOUSE_ACTION_HPP
#define EDITOR2_MOUSE_ACTION_HPP

#include "action_base.hpp"
#include "editor_map.hpp"
#include "../map.hpp"
#include "../terrain.hpp"

class CKey;

namespace editor2 {

/**
 * A mouse action receives events from the controller, and responds to them by creating 
 * an appropriate editor_action object. Mouse actions may store some temporary data
 * such as the last clicked hex for better handling of click-drag. They should *not* modify
 * the map or trigger refreshes, but may set brush locations and similar overlays that
 * should be visible around the mouse cursor, hence the display references are not const.
 */
class mouse_action
{
public:
	mouse_action(const CKey& key)
	: key_(key), toolbar_button_(NULL)
	{
	}

	virtual ~mouse_action() {}
		
	/**
	 * Mouse move (not a drag). Never changes anything (other than temporary highlihts and similar)
	 */
	void move(editor_display& disp, const gamemap::location& hex);
		
	/**
	 * Locations that would be affected by a click, used by move to update highlights. Defauts to higlight the mouseover hex.
	 * Maybe also used for actually performing the action in click() or drag().
	 */
	virtual std::set<gamemap::location> affected_hexes(editor_display& disp, const gamemap::location& hex);
	
	/**
	 * A click, possibly the beginning of a drag. Must be overriden.
	 */
	virtual editor_action* click_left(editor_display& disp, int x, int y) = 0;
	
	/**
	 * A click, possibly the beginning of a drag. Must be overriden.
	 */
	virtual editor_action* click_right(editor_display& disp, int x, int y) = 0;

	/**
	 * Drag operation. A click should have occured earlier. Defaults to no action.
	 */
	virtual editor_action* drag_left(editor_display& disp, int x, int y, bool& partial, editor_action* last_undo);
	
	/**
	 * Drag operation. A click should have occured earlier. Defaults to no action.
	 */
	virtual editor_action* drag_right(editor_display& disp, int x, int y, bool& partial, editor_action* last_undo);

	/**
	 * The end of dragging. Defaults to no action.
	 */
	virtual editor_action* drag_end(editor_display& disp, int x, int y);
	
	/**
	 * Function called by the controller on a key event for the current mouse action.
	 * Defaults to no action.
	 */
	virtual editor_action* key_event(editor_display& disp, const SDL_Event& e);
	
	/**
	 * Helper variable setter - pointer to a toolbar menu/button used for highlighting 
	 * the current action. Should always be NULL or point to a valid menu.
	 */
	void set_toolbar_button(const theme::menu* value) { toolbar_button_ = value; }

	/**
	 * Getter for the (possibly NULL) associated menu/button. 
	 */
	const theme::menu* toolbar_button() const { return toolbar_button_; }
	
	virtual bool uses_terrains() const { return false; }

protected:
	bool has_alt_modifier() const;
	bool has_shift_modifier() const;
	
	/**
	 * The hex previously used in move operations
	 */
	gamemap::location previous_move_hex_;
	
	/**
	 * Key presses, used for modifiers (alt, shift) in some operations
	 */
	const CKey& key_;
	
private:
	/**
	 * Pointer to an associated menu/button, if such exists
	 */
	const theme::menu* toolbar_button_;
};

/**
 * A brush-drag mouse action base class which adds brush and drag processing to a basic mouse action
 */
class brush_drag_mouse_action : public mouse_action
{
public:
	brush_drag_mouse_action(const brush* const * const brush, const CKey& key)
	: mouse_action(key), brush_(brush)
	{
	}
	
	/**
	 * The affected hexes of a brush action are the result of projecting the current brush on the mouseover hex
	 */
	std::set<gamemap::location> affected_hexes(editor_display& disp, const gamemap::location& hex);	
	
	/**
	 * The actual action function which is called by click() and drag(). Derived classes override this instead of click() and drag().
	 */
	virtual editor_action* click_perform_left(editor_display& disp, const std::set<gamemap::location>& hexes) = 0;
	
	/**
	 * The actual action function which is called by click() and drag(). Derived classes override this instead of click() and drag().
	 */
	virtual editor_action* click_perform_right(editor_display& disp, const std::set<gamemap::location>& hexes) = 0;

	/**
	 * Calls click_perform_left()
	 */
	editor_action* click_left(editor_display& disp, int x, int y);

	/**
	 * Calls click_perform_right()
	 */
	editor_action* click_right(editor_display& disp, int x, int y);
	
	/**
	 * Calls click_perform() for every new hex the mouse is dragged into.
	 * @todo partial actions support and merging of many drag actions into one
	 */
	editor_action* drag_left(editor_display& disp, int x, int y, bool& partial, editor_action* last_undo);
	
	/**
	 * Calls click_perform for every new hex the mouse is dragged into.
	 * @todo partial actions support and merging of many drag actions into one
	 */
	editor_action* drag_right(editor_display& disp, int x, int y, bool& partial, editor_action* last_undo);
	
	/**
	 * End of dragging.
	 * @todo partial actions (the entire drag should end up as one action)
	 */
	editor_action* drag_end(editor_display& disp, int x, int y);	
	
protected:
	/** Brush accessor */
	const brush& get_brush();
	
	/**
	 * The previous hex dragged into.
	 * @todo keep a set of all "visited" locations to reduce action count in long drags that hit the same hexes multiple times?
	 */
	gamemap::location previous_drag_hex_;
private:
	/**
	 * Current brush handle. Currently a pointer-to-pointer with full constness.
	 */
	const brush* const * const brush_;
};

/**
 * Brush paint mouse action. Uses keyboard modifiers for one-layer painting.
 */
class mouse_action_paint : public brush_drag_mouse_action
{
public:
	mouse_action_paint(const t_translation::t_terrain& terrain_left, 
		const t_translation::t_terrain& terrain_right, 
		const brush* const * const brush, const CKey& key)
	: brush_drag_mouse_action(brush, key)
	, terrain_left_(terrain_left)
	, terrain_right_(terrain_right)
	{
	}
	
	/**
	 * Create an appropriate editor_action and return it
	 */
	editor_action* click_perform_left(editor_display& disp, const std::set<gamemap::location>& hexes);

	/**
	 * Create an appropriate editor_action and return it
	 */
	editor_action* click_perform_right(editor_display& disp, const std::set<gamemap::location>& hexes);

	bool uses_terrains() const { return true; }
protected:
	const t_translation::t_terrain& terrain_left_;
	const t_translation::t_terrain& terrain_right_;
};

/**
 * Select (and deselect) action, by brush or "magic wand" (via keyboard modifier)
 */
class mouse_action_select : public brush_drag_mouse_action
{
public:
	mouse_action_select(const brush* const * const brush, const CKey& key)
	: brush_drag_mouse_action(brush, key)
	{
	}
	
	/**
	 * Overriden to allow special behaviour based on modifier keys
	 */
	std::set<gamemap::location> affected_hexes(editor_display& disp, const gamemap::location& hex);
	
	/**
	 * Force a fake "move" event to update brush overlay on key event
	 */
	editor_action* key_event(editor_display& disp, const SDL_Event& e);
		
	/**
	 * Left click/drag selects
	 */
	editor_action* click_perform_left(editor_display& disp, const std::set<gamemap::location>& hexes);

	/**
	 * Right click/drag deselects
	 */
	editor_action* click_perform_right(editor_display& disp, const std::set<gamemap::location>& hexes);
};

/**
 * Paste action. No dragging capabilities.
 */
class mouse_action_paste : public mouse_action
{
public:
	mouse_action_paste(const map_fragment& paste, const CKey& key)
	: mouse_action(key), paste_(paste)
	{
	}
	
	/**
	 * Show an outline of where the paste will go
	 */
	std::set<gamemap::location> affected_hexes(editor_display& disp, const gamemap::location& hex);
	
	/**
	 * Return a paste with offset action
	 */
	editor_action* click_left(editor_display& disp, int x, int y);

	/**
	 * Right click does nothing for now
	 */
	editor_action* click_right(editor_display& disp, int x, int y);
	
protected:
	/**
	 * Reference to the buffer used for pasting (e.g. the clipboard)
	 */
	const map_fragment& paste_;
};

/**
 * Fill action. No dragging capabilities. Uses keyboard modifiers for one-layer painting.
 */
class mouse_action_fill : public mouse_action
{
public:
	mouse_action_fill(const t_translation::t_terrain& terrain_left,
		const t_translation::t_terrain& terrain_right, const CKey& key)
	: mouse_action(key)
	, terrain_left_(terrain_left)
	, terrain_right_(terrain_right)
	{
	}
	
	/**
	 * Tiles that will be painted to, possibly use modifier keys here
	 */
	std::set<gamemap::location> affected_hexes(editor_display& disp, const gamemap::location& hex);
	
	/**
	 * Left / right click fills with the respective terrain
	 */
	editor_action* click_left(editor_display& disp, int x, int y);
	
	/**
	 * Left / right click fills with the respective terrain
	 */
	editor_action* click_right(editor_display& disp, int x, int y);
	
	bool uses_terrains() const { return true; }

protected:
	const t_translation::t_terrain& terrain_left_;
	const t_translation::t_terrain& terrain_right_;
};

/**
 * Set starting position action. 
 */
class mouse_action_starting_position : public mouse_action
{
public:
	mouse_action_starting_position(const CKey& key)
	: mouse_action(key)
	{
	}
	
	/**
	 * Allows setting/clearing the starting positions in the mouseover hexes via keyboard
	 */
	editor_action* key_event(editor_display& disp, const SDL_Event& e);
	
	/**
	 * Left click displays a player-number-selector dialog and then creates an action
	 * or returns NULL if cancel was pressed or there would be no change.
	 */
	editor_action* click_left(editor_display& disp, int x, int y);

	/**
	 * Right click only erases the starting position if there is one
	 */
	editor_action* click_right(editor_display& disp, int x, int y);
	
};

} //end namespace editor2

#endif
