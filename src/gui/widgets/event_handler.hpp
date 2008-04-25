/* $Id$ */
/*
   Copyright (C) 2007 - 2008 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

//! @file event_handler.hpp
//! Contains the information with an event.

#ifndef __GUI_WIDGETS_EVENT_INFO_HPP_INCLUDED__
#define __GUI_WIDGETS_EVENT_INFO_HPP_INCLUDED__

#include "events.hpp"
#include "gui/widgets/helper.hpp"

#include "SDL.h"

class t_string;

namespace gui2{

class twidget;
class twindow;

class tevent_handler : public events::handler
{
public:
	tevent_handler();

	virtual ~tevent_handler() { leave(); }

	void process_events() { events::pump(); }

	//! Implement events::handler::handle_event().
	void handle_event(const SDL_Event& event);

	virtual twindow& get_window() = 0;
	virtual const twindow& get_window() const = 0;

	virtual twidget* get_widget(const tpoint& coordinate) = 0;

	void mouse_capture(const bool capture = true);
	void keyboard_capture(twidget* widget) { keyboard_focus_ = widget; }

	tpoint get_mouse() const;

	//! We impement the handling of the tip, but call the do functions
	//! which are virtual.
	void show_tooltip(const t_string& tooltip, const unsigned timeout);
	void remove_tooltip();
	void show_help_popup(const t_string& help_popup, const unsigned timeout);
	void remove_help_popup();

private:
	//! we create a new event context so we're always modal.
	//! Maybe this has to change, but not sure yet.
	events::event_context event_context_;

	int mouse_x_;                      //! The current mouse x.
	int mouse_y_;                      //! The current mouse y.

	bool mouse_left_button_down_;      //! Is the left mouse button down?
	bool mouse_middle_button_down_;    //! Is the middle mouse button down?
	bool mouse_right_button_down_;     //! Is the right mouse button down?

	Uint32 last_left_click_;	
	Uint32 last_middle_click_;	
	Uint32 last_right_click_;	

	bool hover_pending_;			   //! Is there a hover event pending?
	unsigned hover_id_;                //! Id of the pending hover event.
	SDL_Rect hover_box_;               //! The area the mouse can move in, moving outside
	                                   //! invalidates the pending hover event.
    bool had_hover_;                   //! A widget only gets one hover event per enter cycle. 

	//! The widget that created the tooltip / tooltip.
	twidget* tooltip_;
	twidget* help_popup_;


	twidget* mouse_focus_;
	bool mouse_captured_;

	twidget* keyboard_focus_;

	void mouse_enter(const SDL_Event& event, twidget* mouse_over);
	void mouse_move(const SDL_Event& event, twidget* mouse_over);
	void mouse_hover(const SDL_Event& event, twidget* mouse_over);
	void mouse_leave(const SDL_Event& event, twidget* mouse_over);


	void mouse_left_button_down(const SDL_Event& event, twidget* mouse_over);
	void mouse_left_button_up(const SDL_Event& event, twidget* mouse_over);
	void mouse_left_click(twidget* widget);

	void set_hover(const bool test_on_widget = false);

	void key_down(const SDL_Event& event);

	virtual void do_show_tooltip(const tpoint& location, const t_string& tooltip) = 0;
	virtual void do_remove_tooltip() = 0;
	virtual void do_show_help_popup(const tpoint& location, const t_string& help_popup) = 0;
	virtual void do_remove_help_popup() = 0;
};

} // namespace gui2

#endif
