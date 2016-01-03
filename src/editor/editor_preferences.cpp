/*
   Copyright (C) 2009 - 2016 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "editor/editor_preferences.hpp"
#include "config.hpp"
#include "game_preferences.hpp"
#include "serialization/string_utils.hpp"
#include "util.hpp"

#include <boost/foreach.hpp>

namespace preferences {

namespace editor {

	int auto_update_transitions() {
		return lexical_cast_default<int>(preferences::get("editor_auto_update_transitions"), TransitionUpdateMode::partial);
	}

	void set_auto_update_transitions(int value) {
		preferences::set("editor_auto_update_transitions", lexical_cast<std::string>(value));
	}

	std::string default_dir() {
		return preferences::get("editor_default_dir");
	}

	bool draw_terrain_codes() {
		return preferences::get("editor_draw_terrain_codes", false);
	}

	void set_draw_terrain_codes(bool value) {
		preferences::set("editor_draw_terrain_codes", value);
	}

	bool draw_hex_coordinates() {
		return preferences::get("editor_draw_hex_coordinates", false);
	}

	void set_draw_hex_coordinates(bool value) {
		preferences::set("editor_draw_hex_coordinates", value);
	}

	namespace {
		void normalize_editor_rgb(int rval)
		{
			if (rval < -255) {
				rval = -255;
			}
			else if (rval > 255) {
				rval = 255;
			}
		}
	}

	void set_tod_r(int value)
	{
		normalize_editor_rgb(value);
		preferences::set("editor_r",lexical_cast<std::string>(value));
	}

	void set_tod_g(int value)
	{
		normalize_editor_rgb(value);
		preferences::set("editor_g",lexical_cast<std::string>(value));
	}

	void set_tod_b(int value)
	{
		normalize_editor_rgb(value);
		preferences::set("editor_b",lexical_cast<std::string>(value));
	}

	int tod_r()
	{
		return lexical_cast_in_range<int>(preferences::get("editor_r"), 0, -255, 255);
	}

	int tod_g()
	{
		return lexical_cast_in_range<int>(preferences::get("editor_g"), 0, -255, 255);
	}

	int tod_b()
	{
		return lexical_cast_in_range<int>(preferences::get("editor_b"), 0, -255, 255);
	}

	namespace {
		size_t editor_mru_limit()
		{
			return std::max(size_t(1), lexical_cast_default<size_t>(
					preferences::get("editor_max_recent_files"), 10));
		}

		//
		// NOTE: The MRU read/save functions enforce the entry count limit in
		// order to ensure the list on disk doesn't grow forever. Otherwise,
		// normally this would be the UI's responsibility instead.
		//

		std::vector<std::string> do_read_editor_mru()
		{
			const config& cfg = preferences::get_child("editor_recent_files");

			std::vector<std::string> mru;
			if(!cfg) {
				return mru;
			}

			BOOST_FOREACH(const config& child, cfg.child_range("entry"))
			{
				const std::string& entry = child["path"].str();
				if(!entry.empty()) {
					mru.push_back(entry);
				}
			}

			mru.resize(std::min(editor_mru_limit(), mru.size()));

			return mru;
		}

		void do_commit_editor_mru(const std::vector<std::string>& mru)
		{
			config cfg;
			unsigned n = 0;

			BOOST_FOREACH(const std::string& entry, mru)
			{
				if(entry.empty()) {
					continue;
				}

				config& child = cfg.add_child("entry");
				child["path"] = entry;

				if(++n >= editor_mru_limit()) {
					break;
				}
			}

			preferences::set_child("editor_recent_files", cfg);
		}
	}

	std::vector<std::string> recent_files()
	{
		return do_read_editor_mru();
	}

	void add_recent_files_entry(const std::string& path)
	{
		if(path.empty()) {
			return;
		}

		std::vector<std::string> mru = do_read_editor_mru();

		// Enforce uniqueness. Normally shouldn't do a thing unless somebody
		// has been tampering with the preferences file.
		mru.erase(std::remove(mru.begin(), mru.end(), path), mru.end());

		mru.insert(mru.begin(), path);
		mru.resize(std::min(editor_mru_limit(), mru.size()));

		do_commit_editor_mru(mru);
	}

	void remove_recent_files_entry(const std::string& path)
	{
		if(path.empty()) {
			return;
		}

		std::vector<std::string> mru = do_read_editor_mru();

		mru.erase(std::remove(mru.begin(), mru.end(), path), mru.end());

		do_commit_editor_mru(mru);
	}

} //end namespace editor

} //end namespace preferences

