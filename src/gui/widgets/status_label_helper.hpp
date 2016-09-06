/*
   Copyright (C) 2010 - 2016 by the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_STATUS_LABEL_HELPER_HPP_INCLUDED
#define GUI_WIDGETS_STATUS_LABEL_HELPER_HPP_INCLUDED

#include "gui/core/event/dispatcher.hpp"
#include "gui/widgets/control.hpp"

#include "utils/functional.hpp"

namespace gui2 {

/**
 * Creates a bound status label that will reflect the label state of a widget. The initial label
 * value is set here, and then again any time the widget is modified. A function is also returned
 * that can be called to update the label manually.
 *
 * This relies on hooking into the NOTIFY_MODIFIED event, so can only be used with widgets that fire
 * that event.
 */
template<typename W>
std::function<void()> bind_status_label(twidget& find_in, const std::string& id,
		const std::function<std::string(W&)> value_getter = [](W& w)->std::string { return std::to_string(w.get_value()); },
		const std::string& label_id = "")
{
	const std::string label_id_ = label_id.empty() ? id + "_label" : label_id;

	W& source = find_widget<W>(&find_in, id, false);
	tcontrol& label = find_widget<tcontrol>(&find_in, label_id_, false);

	const auto update_label = [&, value_getter]() {
		const std::string value = value_getter(source);

		label.set_label(value);
	};

	// Bind the callback
	connect_signal_notify_modified(source, std::bind(update_label));

	// Set initial value
	update_label();

	return update_label;
}

} // namespace gui2

#endif