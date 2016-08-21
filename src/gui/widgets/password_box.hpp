/*
   Copyright (C) 2009 - 2016 by Thomas Baumhauer
   <thomas.baumhauer@NOSPAMgmail.com>
   Copyright (C) 2009 - 2016 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_PASSWORD_BOX_HPP_INCLUDED
#define GUI_WIDGETS_PASSWORD_BOX_HPP_INCLUDED

#include "gui/widgets/text_box.hpp"


/**
 * A class inherited from ttext_box that displays
 * its input as stars
 *
 * @todo This implementation is quite a hack that
 * needs to be rewritten cleanly
 */
namespace gui2
{

// ------------ WIDGET -----------{

class tpassword_box : public ttext_box
{
public:
	tpassword_box() : ttext_box(), real_value_()
	{
	}

	/** Inherited from ttext_. */
	virtual void set_value(const std::string& text);
	std::string get_real_value() const
	{
		return real_value_;
	}


protected:
	void insert_char(const utf8::string& unicode);
	void paste_selection(const bool mouse);
	void delete_selection();

	// We do not override copy_selection because we
	// actually want it to copy just the stars

private:

	std::string real_value_;

	/** See @ref tcontrol::get_control_type. */
	virtual const std::string& get_control_type() const override;
};

// }---------- BUILDER -----------{

namespace implementation
{

// copy & paste from tbuilder_text_box...
// does it make more sense to inherit from it?
struct tbuilder_password_box : public tbuilder_control
{
public:
	explicit tbuilder_password_box(const config& cfg);

	using tbuilder_control::build;

	twidget* build() const;

private:
	std::string history_;
};

} // namespace implementation

// }------------ END --------------

} // namespace gui2

#endif
