/*
   Copyright (C) 2007 - 2017 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/widgets/settings.hpp"

#include "config_cache.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula/string_utils.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/tips.hpp"
#include "gui/core/log.hpp"
#include "gui/widgets/window.hpp"
#include "preferences/general.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "serialization/schema_validator.hpp"
#include "wml_exception.hpp"

namespace gui2
{
bool new_widgets = false;

namespace settings
{
unsigned screen_width = 0;
unsigned screen_height = 0;

unsigned gamemap_x_offset = 0;

unsigned gamemap_width = 0;
unsigned gamemap_height = 0;

unsigned popup_show_delay = 0;
unsigned popup_show_time = 0;
unsigned help_show_time = 0;
unsigned double_click_time = 0;
unsigned repeat_button_repeat_time = 0;

std::string sound_button_click = "";
std::string sound_toggle_button_click = "";
std::string sound_toggle_panel_click = "";
std::string sound_slider_adjust = "";

t_string has_helptip_message;

static std::vector<game_tip> tips;

std::vector<game_tip> get_tips()
{
	return tip_of_the_day::shuffle(tips);
}

} // namespace settings

/**
 * Notes on the registered widget and window lists.
 *
 * These lists are GUI-independent. They represent the widgets and windows
 * registered from the C++ interface with @ref register_widget or @register_window.
 *
 * Also note these cannot be free-standing static data members within this file since
 * that causes a crash for some reason.
 */

/** Returns the list of registered windows. */
static std::set<std::string>& registered_window_types()
{
	static std::set<std::string> result;
	return result;
}

struct registered_widget_parser
{
	widget_parser_t parser;
	const char* key;
};

using registered_widget_map = std::map<std::string, registered_widget_parser>;

/** Returns the list of registered widgets. */
static registered_widget_map& registered_widget_types()
{
	static registered_widget_map result;
	return result;
}

/**
 * A GUI definiton.
 *
 * Each GUI contains several widgets, their definitons, and windows and controls the appearance and
 * layout of each.

 * Multiple GUI definitions may exist, though only a single default one is provided right now.
 */
class gui_definition
{
public:
	explicit gui_definition(const config& cfg)
		: id(cfg["id"])
		, description(cfg["description"].t_str())
		, widget_types()
		, window_types()
		, popup_show_delay_(0)
		, popup_show_time_(0)
		, help_show_time_(0)
		, double_click_time_(0)
		, repeat_button_repeat_time_(0)
		, sound_button_click_()
		, sound_toggle_button_click_()
		, sound_toggle_panel_click_()
		, sound_slider_adjust_()
		, has_helptip_message_()
		, tips_()
	{
		read(cfg);
	}

	std::string id;
	t_string description;

	/** Activates this gui. */
	void activate() const;

	using styled_widget_definition_map =
		std::map<std::string, std::map<std::string, styled_widget_definition_ptr>>;

	/** Map of each widget type, by id, and a sub-map of each of the type's definitions, also by id. */
	styled_widget_definition_map widget_types;

	/** Map of all known windows (the builder class builds a window). */
	std::map<std::string, builder_window> window_types;

	void load_widget_definitions(
			const std::string& widget_type, const std::vector<styled_widget_definition_ptr>& definitions);

private:
	void read(const config& cfg);

	unsigned popup_show_delay_;
	unsigned popup_show_time_;
	unsigned help_show_time_;
	unsigned double_click_time_;
	unsigned repeat_button_repeat_time_;

	std::string sound_button_click_;
	std::string sound_toggle_button_click_;
	std::string sound_toggle_panel_click_;
	std::string sound_slider_adjust_;

	t_string has_helptip_message_;

	std::vector<game_tip> tips_;
};

/*WIKI
 * @page = GUIToolkitWML
 * @order = 1
 *
 * {{Autogenerated}}
 *
 * = GUI =
 *
 * The gui class contains the definitions of all widgets and windows used in
 * the game. This can be seen as a skin and it allows the user to define the
 * visual aspect of the various items. The visual aspect can be determined
 * depending on the size of the game window.
 *
 * Widgets have a definition and an instance, the definition contains the
 * general info/looks of a widget and the instance the actual looks. Eg the
 * where the button text is placed is the same for every button, but the
 * text of every button might differ.
 *
 * The default gui has the id ''default'' and must exist, in the default gui
 * there must a definition of every widget with the id ''default'' and every
 * window needs to be defined. If the definition of a widget with a certain
 * id doesn't exist it will fall back to default in the current gui, if it's
 * not defined there either it will fall back to the default widget in the
 * default theme. That way it's possible to slowly create your own gui and
 * test it.
 *
 * @begin{parent}{name="/"}
 * @begin{tag}{name="gui"}{min="0"}{max="1"}
 * The gui has the following data:
 * @begin{table}{config}
 *     id & string & &                  Unique id for this gui (theme). $
 *     description & t_string & &       Unique translatable name for this gui. $
 *
 *     widget_types & section & & The definitions of all
 *                                   [[#widget_list|widgets]]. $
 *     window & section & &             The definitions of all
 *                                   [[#window_list|windows]]. $
 *     settings & section & &           The settings for the gui. $
 * @end{table}
 *
 * <span id="widget_list"></span>List of available widgets:
 * @begin{table}{widget_overview}
 * Button &                       @macro = button_description $
 * Image &                        @macro = image_description $
 * Horizontal_listbox &           @macro = horizontal_listbox_description $
 * Horizontal_scrollbar &         @macro = horizontal_scrollbar_description $
 * Label &                        @macro = label_description $
 * Listbox &                      @macro = listbox_description $
 * Minimap &                      @macro = minimap_description $
 * Multi_page &                   @macro = multi_page_description $
 * Panel &                        @macro = panel_description $
 * Password_box &                 A text box masking it's content by asterisks.
 *                                $
 * Repeating_button &             @macro = repeating_button_description $
 * Scroll_label &                 @macro = scroll_label_description $
 * Slider &                       @macro = slider_description $
 * Spacer &                       @macro = spacer_description $
 * Stacked_widget &
 *     A stacked widget is a styled_widget where several widgets can be stacked on top of
 *     each other in the same space. This is mainly intended for over- and
 *     underlays. (The widget is still experimental.) $
 *
 * Text_box &                     A single line text box. $
 * Tree_view &                    @macro = tree_view_description $
 * Toggle_button &
 *     A kind of button with two 'states' normal and selected. This is a more
 *     generic widget which is used for eg checkboxes and radioboxes. $
 *
 * Toggle_panel &
 *     Like a toggle button but then as panel so can hold multiple items in a
 *     grid. $
 *
 * Tooltip &                      A small tooltip with help. $
 * Tree_view &                    A tree view widget. $
 * Vertical_scrollbar &           A vertical scrollbar. $
 * Window &                       A window. $
 * @end{table}
 *
 * @end{tag}{name=gui}
 * @end{parent}{name="/"}
 */

/*WIKI
 * @page = GUIToolkitWML
 * @order = 1
 *
 * @begin{parent}{name="gui/"}
 * @begin{tag}{name="settings"}{min="0"}{max="1"}
 * A setting section has the following variables:
 * @begin{table}{config}
 *     popup_show_delay & unsigned & 0 & The time it take before the popup shows
 *                                     if the mouse moves over the widget. 0
 *                                     means show directly. $
 *     popup_show_time & unsigned & 0 &
 *                                     The time a shown popup remains visible.
 *                                     0 means until the mouse leaves the
 *                                     widget. $
 *     help_show_time & unsigned & 0 & The time a shown help remains visible.
 *                                     0 means until the mouse leaves the
 *                                     widget. $
 *     double_click_time & unsigned & &
 *                                     The time between two clicks to still be
 *                                     a double click. $
 *     repeat_button_repeat_time & unsigned & 0 &
 *                                     The time a repeating button waits before
 *                                     the next event is issued if the button
 *                                     is still pressed down. $
 *
 *     sound_button_click & string & "" &
 *                                     The sound played if a button is
 *                                     clicked. $
 *     sound_toggle_button_click & string & "" &
 *                                     The sound played if a toggle button is
 *                                     clicked. $
 *     sound_toggle_panel_click & string & "" &
 *                                     The sound played if a toggle panel is
 *                                     clicked. Normally the toggle panels
 *                                     are the items in a listbox. If a
 *                                     toggle button is in the listbox it's
 *                                     sound is played. $
 *     sound_slider_adjust & string & "" &
 *                                     The sound played if a slider is
 *                                     adjusted. $
 *
 *     has_helptip_message & t_string & &
 *                                     The string used to append the tooltip
 *                                     if there is also a helptip. The WML
 *                                     variable @$hotkey can be used to get show
 *                                     the name of the hotkey for the help. $
 * @end{table}
 * @end{tag}{name="settings"}
 */

/*WIKI
 * @begin{tag}{name="tip"}{min="0"}{max="-1"}
 * @begin{table}{config}
 *     source & t_string & & Author
 *     text & t_string & & Text of the tip.
 * @end{table}
 * @end{tag}{name="tip"}
 * @end{parent}{name="gui/"}
 */
void gui_definition::read(const config& cfg)
{
	VALIDATE(!id.empty(), missing_mandatory_wml_key("gui", "id"));
	VALIDATE(!description.empty(), missing_mandatory_wml_key("gui", "description"));

	DBG_GUI_P << "Parsing gui " << id << std::endl;

	/** Parse widget definitions of each registered type. */
	for(auto& widget_type : registered_widget_types()) {
		std::vector<styled_widget_definition_ptr> definitions;

		for(const auto& definition : cfg.child_range(
				widget_type.second.key
					? widget_type.second.key
					: widget_type.first + "_definition")
		) {
			definitions.push_back(widget_type.second.parser(definition));
		}

		load_widget_definitions(widget_type.first, definitions);
	}

	/** Parse each window. */
	for(auto& w : cfg.child_range("window")) {
		window_types.emplace(w["id"], builder_window(w));
	}

	if(id == "default") {
		// The default gui needs to define all window types since we're the
		// fallback in case another gui doesn't define the window type.
		for(const auto& window_type : registered_window_types()) {
			const std::string error_msg(
				"Window not defined in WML: '" + window_type + "'."
				"Perhaps a mismatch between data and source versions. Try --data-dir <trunk-dir>");

			VALIDATE(window_types.find(window_type) != window_types.end(), error_msg);
		}
	}

	/***** settings *****/

	/**
	 * @todo Regarding sounds:
	 * Need to evaluate but probably we want the widget definition be able to:
	 * - Override the default (and clear it). This will allow toggle buttons in a
	 *   listbox to sound like a toggle panel.
	 * - Override the default and above per instance of the widget, some buttons
	 *   can give a different sound.
	 */
	const config& settings = cfg.child("settings");

	popup_show_delay_ = settings["popup_show_delay"];
	popup_show_time_ = settings["popup_show_time"];
	help_show_time_ = settings["help_show_time"];
	double_click_time_ = settings["double_click_time"];

	repeat_button_repeat_time_ = settings["repeat_button_repeat_time"];

	VALIDATE(double_click_time_, missing_mandatory_wml_key("settings", "double_click_time"));

	sound_button_click_ = settings["sound_button_click"].str();
	sound_toggle_button_click_ = settings["sound_toggle_button_click"].str();
	sound_toggle_panel_click_ = settings["sound_toggle_panel_click"].str();
	sound_slider_adjust_ = settings["sound_slider_adjust"].str();

	has_helptip_message_ = settings["has_helptip_message"];

	VALIDATE(!has_helptip_message_.empty(), missing_mandatory_wml_key("[settings]", "has_helptip_message"));

	tips_ = tip_of_the_day::load(cfg);
}

void gui_definition::activate() const
{
	settings::popup_show_delay = popup_show_delay_;
	settings::popup_show_time = popup_show_time_;
	settings::help_show_time = help_show_time_;
	settings::double_click_time = double_click_time_;
	settings::repeat_button_repeat_time = repeat_button_repeat_time_;
	settings::sound_button_click = sound_button_click_;
	settings::sound_toggle_button_click = sound_toggle_button_click_;
	settings::sound_toggle_panel_click = sound_toggle_panel_click_;
	settings::sound_slider_adjust = sound_slider_adjust_;
	settings::has_helptip_message = has_helptip_message_;
	settings::tips = tips_;
}

void gui_definition::load_widget_definitions(
		const std::string& widget_type, const std::vector<styled_widget_definition_ptr>& definitions)
{
	for(const auto& def : definitions) {
		if(widget_types[widget_type].find(def->id) != widget_types[widget_type].end()) {
			ERR_GUI_P << "Skipping duplicate styled_widget definition '" << def->id << "' for '" << widget_type
					  << "'\n";
			continue;
		}

		widget_types[widget_type].emplace(def->id, def);
	}

	// The default GUI needs to ensure each widget has a default definition, but
	// non-default GUIs can just fall back to the default definition in the default GUI.
	if(this->id != "default") {
		return;
	}

	const t_string msg(vgettext("Widget definition '$definition' doesn't contain the definition for '$id'.", {
		{"definition", widget_type},
		{"id", "default"}
	}));

	VALIDATE(widget_types[widget_type].find("default") != widget_types[widget_type].end(), msg);
}

/** Map with all known GUIs. */
static std::map<std::string, gui_definition> guis;

/** Points to the current GUI. */
static auto current_gui = guis.end();

/** Points to the default GUI. */
static auto default_gui = guis.end();

void register_window(const std::string& id)
{
	// The second value of emplace is the 'was successfully added' flag.
	if(!registered_window_types().emplace(id).second) {
		WRN_GUI_P << "Window '" << id "' already registered. Ignoring." << std::endl;
	}
}

std::set<std::string> unit_test_access_only::get_registered_window_list()
{
	return gui2::registered_window_types();
}

void load_settings()
{
	LOG_GUI_G << "Setting: init gui." << std::endl;

	// Init.
	window::update_screen_size();

	// Read and validate the WML files.
	config cfg;
	try {
		schema_validation::schema_validator validator(filesystem::get_wml_location("gui/schema.cfg"));

		preproc_map preproc(game_config::config_cache::instance().get_preproc_map());
		filesystem::scoped_istream stream = preprocess_file(filesystem::get_wml_location("gui/_main.cfg"), &preproc);

		read(cfg, *stream, &validator);
	} catch(config::error& e) {
		ERR_GUI_P << e.what() << '\n';
		ERR_GUI_P << "Setting: could not read file 'data/gui/_main.cfg'." << std::endl;
	} catch(const abstract_validator::error& e) {
		ERR_GUI_P << "Setting: could not read file 'data/gui/schema.cfg'." << std::endl;
		ERR_GUI_P << e.message;
	}

	// Parse GUI definitions.
	for(const auto& g : cfg.child_range("gui")) {
		guis.emplace(g["id"], gui_definition(g));
	}

	default_gui = guis.find("default");
	VALIDATE(default_gui != guis.end(), _("No default gui defined."));

	std::string current_theme = preferences::gui_theme();
	current_gui = current_theme.empty() ? default_gui : guis.find(current_theme);

	if(current_gui == guis.end()) {
		ERR_GUI_P << "Missing [gui] definition for '" << current_theme << "'\n";
		current_gui = default_gui;
	}

	current_gui->second.activate();
}

void register_widget(const std::string& id, widget_parser_t f, const char* key)
{
	registered_widget_types()[id] = {f, key};
}

resolution_definition_ptr get_control(const std::string& control_type, const std::string& definition)
{
	const auto& current_types = current_gui->second.widget_types;
	const auto& default_types = default_gui->second.widget_types;

#ifdef GUI2_EXPERIMENTAL_LISTBOX
	const auto widget_definitions = (control_type == "list")
			? current_types.find("listbox")
			: current_types.find(control_type);
#else
	const auto widget_definitions
			= current_types.find(control_type);
#endif

	std::map<std::string, styled_widget_definition_ptr>::const_iterator control;

	if(widget_definitions == current_types.end()) {
		goto fallback;
	}

	control = widget_definitions->second.find(definition);

	if(control == widget_definitions->second.end()) {
	fallback:
		bool found_fallback = false;

		if(current_gui != default_gui) {
#ifdef GUI2_EXPERIMENTAL_LISTBOX
			auto default_widget_definitions = (control_type == "list")
				? default_types.find("listbox")
				: default_types.find(control_type);
#else
			auto default_widget_definitions
				= default_types.find(control_type);
#endif

			VALIDATE(widget_definitions != current_types.end(),
					formatter() << "Type '" << control_type << "' is unknown.");

			control = default_widget_definitions->second.find(definition);
			found_fallback = control != default_widget_definitions->second.end();
		}

		if(!found_fallback) {
			if(definition != "default") {
				LOG_GUI_G << "Control: type '" << control_type << "' definition '" << definition
						  << "' not found, falling back to 'default'.\n";
				return get_control(control_type, "default");
			}

			FAIL(formatter() << "default definition not found for styled_widget " << control_type);
		}
	}

	const auto& resolutions = (*control->second).resolutions;

	VALIDATE(!resolutions.empty(),
		formatter() << "Control: type '" << control_type << "' definition '" << definition << "' has no resolutions.\n");

	for(const auto& res : resolutions) {
		if(settings::screen_width <= res->window_width && settings::screen_height <= res->window_height) {
			return res;
		}
	}

	// If no matching resolution was found, return the last definition.
	return resolutions.back();
}

const builder_window::window_resolution& get_window_builder(const std::string& type)
{
	window::update_screen_size();

	auto window = current_gui->second.window_types.find(type);

	if(window == current_gui->second.window_types.end()) {
		// Current GUI is the default one and no window type was found. Throw.
		if(current_gui == default_gui) {
			throw window_builder_invalid_id();
		}

		// Else, try again to find the window, this time in the default GUI.
		window = default_gui->second.window_types.find(type);

		if(window == default_gui->second.window_types.end()) {
			throw window_builder_invalid_id();
		}
	}

	const auto& resolutions = window->second.resolutions;

	VALIDATE(!resolutions.empty(), formatter() << "Window '" << type << "' has no resolutions.\n");

	for(const auto& res : resolutions) {
		if(settings::screen_width <= res.window_width && settings::screen_height <= res.window_height) {
			return res;
		}
	}

	// If no matching resolution was found, return the last builder.
	return resolutions.back();
}

/*WIKI
 * @page = GUIWidgetDefinitionWML
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 *
 */

bool add_single_widget_definition(const std::string& widget_type, const std::string& definition_id, const config& cfg)
{
	auto& gui = default_gui->second;
	auto parser = registered_widget_types().find(widget_type);

	if(parser == registered_widget_types().end()) {
		throw std::invalid_argument("widget '" + widget_type + "' doesn't exist");
	}

	if(gui.widget_types[widget_type].find(definition_id) != gui.widget_types[widget_type].end()) {
		return false;
	}

	gui.widget_types[widget_type].emplace(definition_id, parser->second.parser(cfg));
	return true;
}

void remove_single_widget_definition(const std::string& widget_type, const std::string& definition_id)
{
	auto& definition_map = default_gui->second.widget_types[widget_type];
	auto it = definition_map.find(definition_id);

	if(it != definition_map.end()) {
		definition_map.erase(it);
	}
}

} // namespace gui2
