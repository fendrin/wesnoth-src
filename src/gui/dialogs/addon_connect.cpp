/* $Id$ */
/*
   copyright (c) 2008 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#include "gui/dialogs/addon_connect.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/window_builder.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/vertical_scrollbar.hpp"
#include "log.hpp"
#include "video.hpp"

#define DBG_GUI LOG_STREAM_INDENT(debug, widget)
#define LOG_GUI LOG_STREAM_INDENT(info, widget)
#define WRN_GUI LOG_STREAM_INDENT(warn, widget)
#define ERR_GUI LOG_STREAM_INDENT(err, widget)

namespace gui2 {

void taddon_connect::show(CVideo& video)
{

	gui2::init();

	twindow window = build(video, get_id(ADDON_CONNECT));
	ttext_box* host_widget = dynamic_cast<ttext_box*>(window.get_widget_by_id("host_name"));
	if(host_widget) {
		host_widget->set_text(host_name_);
		window.keyboard_capture(host_widget);
	}

	tvertical_scrollbar* test_widget = dynamic_cast<tvertical_scrollbar*>(window.get_widget_by_id("test"));
	if(test_widget) {
		std::cerr << "testing scrollbar\n";
		
		test_widget->set_visible_items(10);
		test_widget->set_item_count(200);
		test_widget->set_item_position(0);
		test_widget->set_step_size(1);
	}

	retval_ = window.show(true);

	if(host_widget) {
		if(retval_ == tbutton::OK) {
			host_widget->save_to_history();
		}
		host_name_= host_widget->get_text();
	}
}


} // namespace gui2
