/*
   Copyright (C) 2008 - 2017 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_ADDON_CONNECT_HPP_INCLUDED
#define GUI_DIALOGS_ADDON_CONNECT_HPP_INCLUDED

#include "gui/dialogs/modal_dialog.hpp"

class display;

namespace gui2
{
namespace dialogs
{

/** Addon connect dialog. */
class addon_connect : public modal_dialog
{
public:
	/**
	 * Constructor.
	 *
	 * @param [in, out]host_name  The parameter's usage is:
	 *                            - Input: The initial value for the host_name.
	 *                            - Output :The final value of the host_name if
	 *                              the dialog returns @ref window::OK or 3
	 *                              undefined otherwise.
	 * @param allow_remove        Sets @ref allow_remove_.
	 * @param disp                The display object used for showing the
	 *                            in-game help when requested.
	 */
	addon_connect(std::string& host_name,
				   const bool allow_remove);

private:
	/** Enable the addon remove button? */
	bool allow_remove_;

	void help_button_callback(window& window);

	/** Inherited from modal_dialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from modal_dialog. */
	void pre_show(window& window);

	/** Inherited from modal_dialog. */
	void post_show(window& window);
};

} // namespace dialogs
} // namespace gui2

#endif
