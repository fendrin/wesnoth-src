/*
   Copyright (C) 2008 - 2016 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef EDITOR_EDITOR_CONTROLLER_HPP_INCLUDED
#define EDITOR_EDITOR_CONTROLLER_HPP_INCLUDED

#include "editor/editor_display.hpp"
#include "editor/editor_main.hpp"
#include "editor/map/context_manager.hpp"
#include "editor/map/map_context.hpp"
#include "editor/map/map_fragment.hpp"
#include "editor/toolkit/editor_toolkit.hpp"

#include "controller_base.hpp"
#include "help/help.hpp"
#include "hotkey/command_executor.hpp"
#include "mouse_handler_base.hpp"
#include "tooltips.hpp"

#include "sound_music_track.hpp"

class map_generator;

namespace tooltips {
struct manager;
}

namespace font {
struct floating_label_context;
}

namespace editor {

class editor_map;

std::string get_left_button_function();

enum menu_type {
	MAP,
	LOAD_MRU,
	PALETTE,
	AREA,
	SIDE,
	TIME,
	LOCAL_TIME,
	SCHEDULE,
	LOCAL_SCHEDULE,
	MUSIC,
	UNIT_FACING
};

/**
 * The editor_controller class contains the mouse and keyboard event handling
 * routines for the editor. It also serves as the main editor class with the
 * general logic.
 */
class editor_controller : public controller_base,
	public hotkey::command_executor_default,
	public events::mouse_handler_base,
	private boost::noncopyable,
	quit_confirmation
{
	public:
		/**
		 * The constructor. A initial map context can be specified here, the controller
		 * will assume ownership and delete the pointer during destruction, but changes
		 * to the map can be retrieved between the main loop's end and the controller's
		 * destruction.
		 */
		editor_controller(const config &game_config, CVideo& video);

		~editor_controller();

		/** Editor main loop */
		EXIT_STATUS main_loop();

		/** Takes a screenshot **/
		void do_screenshot(const std::string& screenshot_filename = "map_screenshot.bmp");

		/** Process a hotkey quit command */
		void hotkey_quit();

		/** Show a quit confirmation dialog and returns true if the user pressed 'yes' */
		bool quit_confirm();

		/** Display the settings dialog, used to control e.g. the lighting settings */
		void custom_tods_dialog();

		/** Save the map, open dialog if not named yet. */
		void save_map() {context_manager_->save_map();}

		/** command_executor override */
		bool can_execute_command(const hotkey::hotkey_command& command, int index = -1) const;

		/** command_executor override */
		hotkey::ACTION_STATE get_action_state(hotkey::HOTKEY_COMMAND command, int index) const;

		/** command_executor override */
		bool execute_command(const hotkey::hotkey_command& command, int index = -1, bool press=true);

		/** controller_base override */
		void show_menu(const std::vector<std::string>& items_arg, int xloc, int yloc, bool context_menu, display& disp);

		void show_help();
		void status_table();

		/** Show the preferences dialog */
		void preferences();

		/** Handle hotkeys to scroll map */
		void scroll_up(bool on);
		void scroll_down(bool on);
		void scroll_left(bool on);
		void scroll_right(bool on);

		/** Grid toggle */
		void toggle_grid();

		void terrain_description();
		void unit_description();
		void change_unit_id();
		void rename_unit();

		void unit_list();

		/** Copy the selection on the current map to the clipboard */
		void copy_selection();

		/** Cut the selection from the current map to the clipboard */
		void cut_selection();

		/** Export the WML-compatible list of selected tiles to the system clipboard */
		void export_selection_coords();

		void update_mouse_action_highlights();

		/** Save the current selection to the active area. */
		void save_area();

		/** Add a new area to the current context, filled with the selection if any. */
		void add_area();

		/* mouse_handler_base overrides */
		void mouse_motion(int x, int y, const bool browse, bool update, map_location new_loc = map_location::null_location());
		editor_display& gui() { return *gui_; }
		const editor_display& gui() const { return *gui_; }
		bool allow_mouse_wheel_scroll(int x, int y);
		bool right_click_show_menu(int x, int y, const bool browse);
		bool left_click(int x, int y, const bool browse);
		void left_drag_end(int x, int y, const bool browse);
		void left_mouse_up(int x, int y, const bool browse);
		bool right_click(int x, int y, const bool browse);
		void right_drag_end(int x, int y, const bool browse);
		void right_mouse_up(int x, int y, const bool browse);

		virtual hotkey::command_executor * get_hotkey_command_executor();

	protected:
		/* controller_base overrides */
		void process_keyup_event(const SDL_Event& event);
		mouse_handler_base& get_mouse_handler_base() { return *this; }
		editor_display& get_display() { return *gui_; }

		/** Get the current mouse action */
		mouse_action* get_mouse_action();

		/**
		 * Perform an action, then delete the action object.
		 * The pointer can be nullptr, in which case nothing will happen.
		 */
		void perform_delete(editor_action* action);

		/**
		 * Peform an action on the current map_context, then refresh the display
		 * and delete the pointer. The pointer can be nullptr, in which case nothing will happen.
		 */
		void perform_refresh_delete(editor_action* action, bool drag_part = false);


		virtual std::vector<std::string> additional_actions_pressed() override;

	private:

		/** init the display object and general set-up */
		void init_gui();

		/** init the available time-of-day settings */
		void init_tods(const config& game_config);

		/** init background music for the editor */
		void init_music(const config& game_config);

		/** Load editor-specific tooltips */
		void load_tooltips();

		/** Reload images */
		void refresh_image_cache();

		/**
		 * Callback function passed to display to be called on each redraw_everything run.
		 * Redraws toolbar, brush bar and related items.
		 */
		void display_redraw_callback(display&);

		/**
		 * Undos an action in the current map context
		 */
		void undo();

		/**
		 * Redos an action in the current map context
		 */
		void redo();

		editor::menu_type active_menu_;

		/** Reports object. Must be initialized before the gui_ */
		const std::unique_ptr<reports> reports_;

		/** The display object used and owned by the editor. */
		const std::unique_ptr<editor_display> gui_;

		/** Pre-defined time of day lighting settings for the settings dialog */
		typedef std::map<std::string, std::pair<std::string ,std::vector<time_of_day> > > tods_map;
		tods_map tods_;

		/* managers */
	public:
		const std::unique_ptr<context_manager> context_manager_;
	private:
		std::unique_ptr<editor_toolkit> toolkit_;
		tooltips::manager tooltip_manager_;
		std::unique_ptr<font::floating_label_context> floating_label_manager_;

		std::unique_ptr<help::help_manager> help_manager_;

		/** Quit main loop flag */
		bool do_quit_;
		EXIT_STATUS quit_mode_;

		std::vector<sound::music_track> music_tracks_;
};

} //end namespace editor

#endif
