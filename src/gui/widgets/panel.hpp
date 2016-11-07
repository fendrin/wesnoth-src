/*
   Copyright (C) 2008 - 2016 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_PANEL_HPP_INCLUDED
#define GUI_WIDGETS_PANEL_HPP_INCLUDED

#include "gui/widgets/container.hpp"

#include "gui/core/widget_definition.hpp"
#include "gui/core/window_builder.hpp"

namespace gui2
{

// ------------ WIDGET -----------{

/**
 * Visible container to hold multiple widgets.
 *
 * This widget can draw items beyond the widgets it holds and in front of them.
 * A panel is always active so these functions return dummy values.
 */
class tpanel : public tcontainer_
{

public:
	/**
	 * Constructor.
	 *
	 * @param canvas_count        The canvas count for tcontrol.
	 */
	explicit tpanel(const unsigned canvas_count = 2) : tcontainer_(canvas_count)
	{
	}

	/** See @ref tcontainer_::get_client_rect. */
	virtual SDL_Rect get_client_rect() const override;

	/** See @ref tcontrol::get_active. */
	virtual bool get_active() const override;

	/** See @ref tcontrol::get_state. */
	virtual unsigned get_state() const override;

private:
	/** See @ref twidget::impl_draw_background. */
	virtual void impl_draw_background(surface& frame_buffer,
									  int x_offset,
									  int y_offset) override;

	/** See @ref twidget::impl_draw_foreground. */
	virtual void impl_draw_foreground(surface& frame_buffer,
									  int x_offset,
									  int y_offset) override;

	/** See @ref tcontrol::get_control_type. */
	virtual const std::string& get_control_type() const override;

	/** See @ref tcontainer_::border_space. */
	virtual point border_space() const override;

	/** See @ref tcontainer_::set_self_active. */
	virtual void set_self_active(const bool active) override;
};

// }---------- DEFINITION ---------{

struct panel_definition : public control_definition
{
	explicit panel_definition(const config& cfg);

	struct tresolution : public resolution_definition
	{
		explicit tresolution(const config& cfg);

		unsigned top_border;
		unsigned bottom_border;

		unsigned left_border;
		unsigned right_border;
	};
};

// }---------- BUILDER -----------{

namespace implementation
{

struct builder_panel : public builder_control
{
	explicit builder_panel(const config& cfg);

	using builder_control::build;

	twidget* build() const;

	builder_grid_ptr grid;
};

} // namespace implementation

// }------------ END --------------

} // namespace gui2

#endif
