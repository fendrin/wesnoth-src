/* $Id$ */
/*
   Copyright (C) 2006 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "log.hpp"
#include "terrain_translation.hpp"
#include "serialization/string_utils.hpp"
#include "wassert.hpp"

#include <iostream>

#define ERR_G  LOG_STREAM(err, general)
#define WRN_G  LOG_STREAM(warn, general)

namespace t_translation {

/***************************************************************************************/
// forward declaration of internal functions
	
#ifdef TERRAIN_TRANSLATION_COMPATIBLE 

	// the former terrain letter
	typedef char TERRAIN_LETTER;

	// This function can convert EOL's and converts them to EOL 
	// which doesn't need to be and EOL char
	// this will convert UNIX, Mac and Windows end of line types
	// this due to the fact they all have a different idea of EOL
	// Note this also eats all blank lines so the sequence "\n\n\n" will become just 1 EOL
	t_list string_to_vector_(const std::string& str, const bool convert_eol, const int separated);

	// When the terrain is loaded it sends all letter, string combinations
	// to add_translation. This way the translation table is build.
	// This way it's possible to read old maps and convert them to
	// the proper internal format
	static std::map<TERRAIN_LETTER, t_letter> lookup_table_;
	
	// This value contains the map format used, when reading the main
	// map this format should be set, don't know how we're going to do
	// it but we will. This format determines whether the WML
	// map and letter are read old or new format.
	// formats
	// 0 = unknown
	// 1 = old single letter format
	// 2 = new multi letter format
	static int map_format_ = 0;

	//old low level converters
	t_letter letter_to_number_(const TERRAIN_LETTER terrain); 

	// reads old maps
	t_map read_game_map_old_(const std::string& map,std::map<int, coordinate>& starting_positions); 

	//this one is used privately only for error messages used in string_to_number_ 
	//so we can't use this function to convert us. So we do the conversion here 
	//manually not the niced solution but good enough for a tempory solution
	const t_letter KEEP = ((t_letter)'_' << 24) + ((t_letter)'K' << 18);

#endif

	// the low level convertors, these function are the ones which
	// now about the internal format. All other functions are unaware
	// of the internal format
	
	/**
	 * This is the new convertor converts a single line
	 * and only acceptes the new terrain string format
	 *
	 * @param str	the terrain string in new format
	 *
	 * @return the list of converted terrains
	 */
	t_list string_to_vector_(const std::string& str);
	
	/**
	 * Gets a mask for a terrain, this mask is used for wildcard matching
	 *
	 * @param terrain 	the terrain which might have a wildcard
	 *
	 * @return the mask for this terrain
	 */
	t_letter get_mask_(const t_letter terrain);

	/**
	 * converts a terrain string to a number
	 * @param terrain 			the terrain with an optional number
	 * 
	 * @param start_position	returns the start_position, the caller should set it on -1
	 * 					    	and it's only changed it there is a starting position found
	 *
	 * @return					the letter found in the string
	 */ 					
	t_letter string_to_number_(const std::string& str);
	t_letter string_to_number_(std::string str, int& start_position);

	/**
	 * converts a terrain number to a string
	 * 
	 * @param terrain				the terrain number to convert
	 * @param starting_position		the starting position, if smaller than 0 it's
	 * 								ignored else it's written
	 * @returnn						the converted string, if no starting position 
	 * 								given it's padded to 4 chars else padded to 7 chars
	 */
	std::string number_to_string_(t_letter terrain, const int start_position = -1);

	/**
	 * converts a terrain string to a letter for the builder the translation 
	 * rules differ from the normal conversion rules
	 *
	 * @param str	the terrain string
	 *
	 * @return		number for the builder map
	 */
	t_letter string_to_builder_number_(std::string str);

/***************************************************************************************/	

const t_letter VOID_TERRAIN = string_to_number_("_s");
const t_letter FOGGED = string_to_number_("_f");

const t_letter CASTLE = string_to_number_("Ch");
const t_letter HUMAN_KEEP = string_to_number_("Kh");
const t_letter SHALLOW_WATER = string_to_number_("Ww");
const t_letter DEEP_WATER = string_to_number_("Wo");
const t_letter GRASS_LAND = string_to_number_("Gg");
const t_letter FOREST = string_to_number_("Ff");
const t_letter MOUNTAIN = string_to_number_("Mm");
const t_letter HILL = string_to_number_("Hh");

const t_letter CAVE_WALL = string_to_number_("Xu");
const t_letter CAVE = string_to_number_("Uu");
const t_letter UNDERGROUND_VILLAGE = string_to_number_("Vu");
const t_letter DWARVEN_CASTLE = string_to_number_("Cud");

const t_letter PLUS = string_to_number_("+");
const t_letter MINUS = string_to_number_("-");
//if only used in terrain match code it can move here since 
//we will contain terrain match code
const t_letter NOT = string_to_number_("!");

const t_letter COMMA = ','; //to be translated? or obsoleted

const t_letter STAR = string_to_number_("*");

// the shift used for the builder (needs more comment)
const int BUILDER_SHIFT = 8;

// constant for no wildcard used at multiple places thus 
// defined here. The other masks are only used without
// other functions knowing their value, so they're not
// defined here
enum{ WILDCARD_NONE = 0xFFFFFFFF };

/***************************************************************************************/	

t_match::t_match(const std::string& str):
	terrain(t_translation::read_list(str, -1, t_translation::T_FORMAT_STRING)) 
{
	mask.resize(terrain.size());
	masked_terrain.resize(terrain.size());
	has_wildcard = t_translation::has_wildcard(terrain);

	for(size_t i = 0; i < terrain.size(); i++) {
		mask[i] = t_translation::get_mask_(terrain[i]);
		masked_terrain[i] = mask[i] & terrain[i];
	}
}

t_letter read_letter(const std::string& str, const int t_format)
{
//	throw error("empty strings not allowed in read_letter"); FIXME MdW evaluate this it aborts the editor
#ifdef TERRAIN_TRANSLATION_COMPATIBLE 
	if(t_format == T_FORMAT_STRING ||
			(t_format == T_FORMAT_AUTO && map_format_ == 2)) {
		return string_to_number_(str);
		
	} else if(t_format == T_FORMAT_LETTER ||
			(t_format == T_FORMAT_AUTO && map_format_ == 1)) {
		return letter_to_number_(str[0]);
		
	} else {
		throw error("Invalid case in read_letter");
	}
#else
		return string_to_number_(str);
#endif
}

std::string write_letter(const t_letter letter)
{
	return number_to_string_(letter);
}

t_list read_list(const std::string& str, const int separated, const int t_format)
{
#ifdef TERRAIN_TRANSLATION_COMPATIBLE 
	if(t_format == T_FORMAT_STRING ||
			(t_format == T_FORMAT_AUTO && map_format_ == 2)) {
		return string_to_vector_(str);
		
	} else if(t_format == T_FORMAT_LETTER ||
			(t_format == T_FORMAT_AUTO && map_format_ == 1)) {
		return string_to_vector_(str, false, separated);
		
	} else {
		throw error("Invalid case in read_list");
	}
#else
		return string_to_vector_(str);
#endif
}

std::string write_list(const t_list& list)
{
	std::stringstream result; 

	t_list::const_iterator itor = list.begin();
	for( ; itor != list.end(); ++itor) {
		if(itor == list.begin()) {
			result << number_to_string_(*itor);
		} else {
			result << ", " << number_to_string_(*itor);
		}
	}

	return result.str();
}

t_map read_game_map(const std::string& str,	std::map<int, coordinate>& starting_positions)
{
#ifdef TERRAIN_TRANSLATION_COMPATIBLE 

	// process the data, polls for the format. NOTE we test for a comma
	// so an empty map or a map with 1 letter is doomed to be the old
	// format. Shouldn't hurt
	if(str.find(',') == std::string::npos) {
		//old format
		ERR_G << "Using the single letter map format is deappricated\n";
		map_format_ = 1;
		return read_game_map_old_(str, starting_positions);
	}

	// at this point we're the new format, we also dissapear in the future so
	// inside the ifdef
	map_format_ = 2;
#endif
	
	size_t offset = 0;
	size_t x = 0, y = 0, width = 0;
	t_map result = t_map();

	// skip the leading newlines
	while(offset < str.length() && utils::isnewline(str[offset])) {
		++offset;
	}
	
	// did we get an empty map?
	if((offset + 1) >= str.length()) {
		WRN_G << "Empty map found\n";
		return result;
	}
		
	while(offset < str.length()) {

		// get a terrain chunk
		const std::string separators = ",\n\r";
		const int pos_separator = str.find_first_of(separators, offset);
		const std::string terrain = str.substr(offset, pos_separator - offset);

		// process the chunk
		int starting_position = -1; 
		const t_letter tile = string_to_number_(terrain, starting_position);

		// add to the resulting starting position
		if(starting_position != -1) {
			if(starting_positions.find(starting_position) != starting_positions.end()) {
				// redefine existion position
				WRN_G << "Starting position " << starting_position <<" is redefined.\n";
				starting_positions[starting_position].x = x;
				starting_positions[starting_position].y = y;
			} else {
				// add new position
				struct coordinate coord = {x, y};
				starting_positions.insert(std::pair<int, coordinate>(starting_position, coord));
			}
		} 

		// make space for the new item 
		// NOTE we increase the vector every loop for every x and y profiling
		// with an increase of y with 256 items didn't show an significant speed
		// increase. So didn't rework the system to allocate larger vectors at 
		// once. 
		if(result.size() <= x) {
			result.resize(x + 1);
		}
		if(result[x].size() <= y) {
			result[x].resize(y + 1);
		}
		
		// add the resulting terrain number
		result[x][y] = tile;

		//evaluate the separator
		if(utils::isnewline(str[pos_separator])) {
			// the first line we set the with the other lines we check the width
			if(y == 0 ) { 
				// x contains the offset in the map
				width = x + 1;
			} else {
				if((x + 1) != width ) {
					ERR_G << "Map not a rectangle error occured at line offset " << y << " position offset " << x << "\n"; 
					throw error("Map not a rectangle.");
				}
			}

			// prepare next itertration 
			++y;
			x = 0;
			
			offset =  pos_separator + 1;
			//skip the following newlines 
			//FIXME this should be documented "aa<CR><CR>bb<CR><CR><CR>" is now valid
			while(offset < str.length() && utils::isnewline(str[offset])) {
				++offset;
			}

		} else {
			++x;
			offset = pos_separator + 1;
		}

	}

	if(x != 0 && (x + 1) != width) {
		ERR_G << "Map not a rectangle error occured at the end\n"; 
		throw error("Map not a rectangle.");
	}
	
	return result;
}

std::string write_game_map(const t_map& map, std::map<int, coordinate> starting_positions)
{
	std::stringstream str;

	for(size_t y = 0; y < map[0].size(); ++y) {
		for(size_t x = 0; x < map.size(); ++x) {

			// If the current location is a starting position it needs to
			// be added to the terrain. After it's found it can't be found
			// again so the location is removed from the map.
			std::map<int, coordinate>::iterator itor = starting_positions.begin();
			int starting_position = -1;
			for(; itor != starting_positions.end(); ++itor) {
				if(itor->second.x == x && itor->second.y == y) {
					starting_position = itor->first;
					starting_positions.erase(itor);
					break;
				}
			}

			// add the separator
			if(x != 0) {
				str << ", ";
			}
			str << number_to_string_(map[x][y], starting_position);
		}

		str << "\n";
	}

	return str.str();
}

bool terrain_matches(const t_letter src, const t_letter dest)
{
	return terrain_matches(src, t_list(1, dest));
}

bool terrain_matches(const t_letter src, const t_list& dest)
{
	//NOTE we impose some code duplication we could have been rewritten
	//to get a match structure and then call the version with the match
	//structure. IMO that's some extra overhead to this function which is
	//not required. Hence the two versions
	if(dest.empty()) {
		return false;
	}

	const t_letter star = STAR;
	const t_letter inverse = NOT;
	
	const t_letter src_mask = get_mask_(src);
	const t_letter masked_src = (src & src_mask);
	const bool src_has_wildcard = has_wildcard(src);

	bool result = true;
	t_list::const_iterator itor = dest.begin();

	// try to match the terrains if matched jump out of the loop.
	for(; itor != dest.end(); ++itor) {

		// match wildcard 
		if(*itor == star) {
			return result;
		}

		// match inverse symbol
		if(*itor == inverse) {
			result = !result;
			continue;
		}

		// full match 
		if(src == *itor) {
			return result;
		}
		
		// does the source wildcard match
		if(src_has_wildcard && (*itor & src_mask) == masked_src) {
			return result;
		}
		
		// does the destination wildcard match
		const t_letter dest_mask = get_mask_(*itor);
		const t_letter masked_dest = (*itor & dest_mask);
		if((src & dest_mask) == masked_dest) {
			return result;
		}
	}

	// no match, return the inverse of the result
	return !result;
}

bool terrain_matches(const t_letter src, const t_match& dest)
{
	if(dest.terrain.empty()) {
		return false;
	}

	const t_letter star = STAR;
	const t_letter inverse = NOT;

	const t_letter src_mask = get_mask_(src);
	const t_letter masked_src = (src & src_mask);
	const bool src_has_wildcard = has_wildcard(src);

	bool result = true;

	// try to match the terrains if matched jump out of the loop.
	for(size_t i = 0; i < dest.terrain.size(); ++i) {

		// match wildcard 
		if(dest.terrain[i] == star) {
			return result;
		}

		// match inverse symbol
		if(dest.terrain[i] == inverse) {
			result = !result;
			continue;
		}

		// full match 
		if(dest.terrain[i] == src) {
			return result;
		}
		
		// does the source wildcard match
		if(src_has_wildcard && (dest.terrain[i] & src_mask) == masked_src) {
			return result;
		}
		
		// does the destination wildcard match
		if(dest.has_wildcard && (src & dest.mask[i]) == dest.masked_terrain[i]) {
			return result;
		}
	}

	// no match, return the inverse of the result
	return !result;
}

bool has_wildcard(const t_letter letter) 
{
	return has_wildcard(t_list(1, letter));
}

bool has_wildcard(const t_list& list)
{
	if(list.empty()) {
		return false;
	}
	
	// test all items for a wildcard 
	t_list::const_iterator itor = list.begin();
	for(; itor != list.end(); ++itor) {
		if(get_mask_(*itor) != WILDCARD_NONE) {
			return true;
		}
	}

	// no wildcard found
	return false;
}

t_map read_builder_map(const std::string& str)
{
	size_t offset = 0;
	t_map result = t_map();

	// skip the leading newlines
	while(offset < str.length() && utils::isnewline(str[offset])) {
		++offset;
	}
	
	// did we get an empty map?
	if((offset + 1) >= str.length()) {
		WRN_G << "Empty map found\n";
		return result;
	}
		
	size_t x = 0, y = 0;
	while(offset < str.length()) {

		// get a terrain chunk
		const std::string separators = ",\n\r";
		const size_t pos_separator = str.find_first_of(separators, offset);

		std::string terrain = "";
		// make sure we didn't hit and empty chunk
		// which is allowed
		if(pos_separator != offset) {
			terrain = str.substr(offset, pos_separator - offset);
		}

		// process the chunk
		const t_letter tile = string_to_builder_number_(terrain);

		// make space for the new item
		if(result.size() <= y) {
			result.resize(y + 1);
		}
		if(result[y].size() <= x) {
			result[y].resize(x + 1);
		}
		
		// add the resulting terrain number,
		result[y][x] = tile;

		//evaluate the separator
		if(pos_separator == std::string::npos) {
			// probably not required to change the value but be sure
			// the case should be handled at least. I'm not sure how
			// it defined in the standard but here it's defined at 
			// max u32 which with +1 gives 0 and make a nice infinite 
			// loop.
			offset = str.length();	
		} else if(utils::isnewline(str[pos_separator])) {
			// prepare next itertration 
			++y;
			x = 0;
			
			offset =  pos_separator + 1;
			//skip the following newlines 
			//FIXME MdW this should be documented "aa<CR><CR>bb<CR><CR><CR>" is now valid
			while(offset < str.length() && utils::isnewline(str[offset])) {
				++offset;
			}

		} else {
			++x;
			offset = pos_separator + 1;
		}

	}

	return result;
}

t_letter cast_to_builder_number(const t_letter terrain) 
{
		return (terrain >> BUILDER_SHIFT); 
}

#ifdef TERRAIN_TRANSLATION_COMPATIBLE 
void add_translation(const std::string& str, const t_letter number)
{
	// if the table is empty we need to load some
	// hard-coded values for the custom terrains
	// FIXME: remove this after Wesnoth 1.4 or 2.0
	// FIXME MdW this initial table should be obsolete right now so remove it
	/*
	if(lookup_table_.empty()) {
		lookup_table_.insert(std::pair<TERRAIN_LETTER, t_letter>(' ', VOID_TERRAIN));	
		lookup_table_.insert(std::pair<TERRAIN_LETTER, t_letter>('~', FOGGED));	
		//UMC
		lookup_table_.insert(std::pair<TERRAIN_LETTER, t_letter>('^', string_to_number_("_za")));	
		lookup_table_.insert(std::pair<TERRAIN_LETTER, t_letter>('@', string_to_number_("_zb")));	
		lookup_table_.insert(std::pair<TERRAIN_LETTER, t_letter>('x', string_to_number_("_zx")));	
		lookup_table_.insert(std::pair<TERRAIN_LETTER, t_letter>('y', string_to_number_("_zy")));	
		lookup_table_.insert(std::pair<TERRAIN_LETTER, t_letter>('z', string_to_number_("_zz")));	
	}
	*/

	// we translate the terrain manual since the helper functions use the 
	// translation table and give chicken and egg problem
	const TERRAIN_LETTER letter = str[0];
	std::map<TERRAIN_LETTER, t_letter>::iterator index = lookup_table_.find(letter);
	
	if(index == lookup_table_.end()) {
		// add new item
		lookup_table_.insert(std::pair<TERRAIN_LETTER, t_letter>(letter, number));
	} else {
		// replace existing item
		WRN_G << "Old terrain letter " << letter <<" is redefined.\n";
		index->second = number;
	}
}
#endif

/***************************************************************************************/	
//internal

#ifdef TERRAIN_TRANSLATION_COMPATIBLE 

t_list string_to_vector_(const std::string& str, const bool convert_eol, const int separated)
{
	// only used here so define here
	const t_letter EOL = 7;
	bool last_eol = false;
	t_list result = t_list(); 

	std::string::const_iterator itor = str.begin();
	for( ; itor != str.end(); ++itor) {
		
		if(separated == 1 && *itor == ',') {
			//ignore the character
			last_eol = false;
			
		} else if ((convert_eol) && (*itor == '\n' || *itor == '\r')) {
			// end of line marker found
			if(last_eol == false){
				// last wasn't eol then add us
				result.push_back(EOL);
			}
			last_eol = true;
			
		} else {
			// normal just add
			last_eol = false;
			result.push_back(letter_to_number_(*itor));
		}
	}

	return result;
}

t_letter letter_to_number_(const TERRAIN_LETTER terrain)
{
	std::map<TERRAIN_LETTER, t_letter>::const_iterator itor = lookup_table_.find(terrain);

	if(itor == lookup_table_.end()) {
		ERR_G << "No translation found for old terrain letter " << terrain << "\n";
		throw error("No translation found for old terrain letter");
	}

	return itor->second;
}

t_map read_game_map_old_(const std::string& str, std::map<int, coordinate>& starting_positions) 
{
	size_t offset = 0;
	size_t x = 0, y = 0, width = 0;
	t_map result;
	
	// skip the leading newlines
	while(offset < str.length() && utils::isnewline(str[offset])) {
		++offset;
	}

	// did we get an empty map?
	if((offset + 1) >= str.length()) {
		WRN_G << "Empty map found\n";
		return result;
	}
	
	while(offset < str.length()) {

		// handle newlines
		if(utils::isnewline(str[offset])) {
			// the first line we set the with the other lines we check the width
			if(y == 0 ) { 
				// note x has been increased to the new value at the end of 
				// the loop so width = x and not x + 1
				width = x;
			} else {
				if(x != width ) {
					ERR_G << "Map not a rectangle error occured at line " << y << " position " << x << "\n"; 
					throw error("Map not a rectangle.");
				}
			}

			// prepare next itertration 
			++y;
			x = 0;
			++offset;
			
			//skip the following newlines 
			while(offset < str.length() && utils::isnewline(str[offset])) {
				++offset;
			}
			
			// stop if at end of file
			if((offset + 1) >= str.length()) {
				break;
			}
		}
		
		// get a terrain chunk
		TERRAIN_LETTER terrain = str[offset];

		// process the chunk
		int starting_position = lexical_cast_default<int>(std::string(1, terrain), -1);
		
		// add to the resulting starting position
		if(starting_position != -1) {
			if(starting_positions.find(starting_position) != starting_positions.end()) {
				// redefine existion position
				WRN_G << "Starting position " << starting_position <<" redefined.\n";
				starting_positions[starting_position].x = x;
				starting_positions[starting_position].y = y;
			} else {
				// add new position
				struct coordinate coord = {x, y};
				starting_positions.insert(std::pair<int, coordinate>(starting_position, coord));
			}
			//the letter of the keep hardcoded, since this code is 
			//scheduled for removal the hardcoded letter is oke
			terrain = 'K';
		} 
		
		// make space for the new item
		if(result.size() <= x) {
			result.resize(x + 1);
		}
		if(result[x].size() <= y) {
			result[x].resize(y + 1);
		}
		
		// add the resulting terrain number,
		result[x][y] = letter_to_number_(terrain);

		//set next value
		++x; 
		++offset;
	}

	if(x != 0 && x != width) {
		ERR_G << "Map not a rectangle error occured at the end\n"; 
		throw error("Map not a rectangle.");
	}

	return result;
}

#endif

t_list string_to_vector_(const std::string& str)
{
	// handle an empty string
	t_list result = t_list();

	if(str.empty()) {
		return result;
	}
		
	size_t offset = 0;
	while(offset < str.length()) {

		// get a terrain chunk
		const std::string separators = ",";
		const size_t pos_separator = str.find_first_of(separators, offset);
		const std::string terrain = str.substr(offset, pos_separator - offset);

		// process the chunk
		const t_letter tile = string_to_number_(terrain);

		// add the resulting terrain number
		result.push_back(tile);

		//evaluate the separator
		if(pos_separator == std::string::npos) {
			offset =  str.length();
		} else {
			offset = pos_separator + 1;
		}
	}

	return result;
}

t_letter get_mask_(const t_letter terrain)
{
	// NOTE this code was first written in a loop, it looked ugly and was slow
	// g++ won't unroll-loops by default and older versions also optimize no so
	// good. The code change almost doubled the speed of the routine.
	
	// test for the star 0x2A in every postion and return the
	// appropriate mask
	if((terrain & 0xFF000000) == 0x2A000000) return 0x00000000;
	if((terrain & 0x00FF0000) == 0x002A0000) return 0xFF000000;
	if((terrain & 0x0000FF00) == 0x00002A00) return 0xFFFF0000;
	if((terrain & 0x000000FF) == 0x0000002A) return 0xFFFFFF00;

	// no star found return the default
	return WILDCARD_NONE;
}

t_letter string_to_number_(const std::string& str) {
	int dummy = -1;
	return string_to_number_(str, dummy);
}

t_letter string_to_number_(std::string str, int& start_position)
{
	t_letter result = NONE_TERRAIN;

	//strip the spaces around us
	const std::string& whitespace = " \t";
	str.erase(0, str.find_first_not_of(whitespace));
	str.erase(str.find_last_not_of(whitespace) + 1);

	// split if we have 1 space inside
	const size_t offset = str.find(' ', 0);
	if(offset != std::string::npos) {
		start_position = lexical_cast<int>(str.substr(0, offset));
		str.erase(0, offset + 1);
	}

	//the conversion to int puts the first char in the 
	//highest part of the number, this will make the 
	//wildcard matching later on a bit easier.
	for(size_t i = 0; i < 4; ++i) {
		const unsigned char c = (i < str.length()) ? str[i] : 0;
		
		// clear the lower area is a nop on i == 0 so 
		// no need for if statement
		result <<= 8; 
		
		// add the result
		result += c;
	}

#ifdef TERRAIN_TRANSLATION_COMPATIBLE 
	if(result == KEEP) {
		ERR_G << "Using _K for a keep is deappricated\n";
	}
#endif
	
	return result;
}

std::string number_to_string_(t_letter terrain, const int start_position)
{
	std::string result = "";

	//insert the start position
	if(start_position > 0) {
		result = str_cast(start_position) + " ";
	}
	
	//insert the terrain letter
	unsigned char letter[4];
	letter[0] = ((terrain & 0xFF000000) >> 24);
	letter[1] = ((terrain & 0x00FF0000) >> 16);
	letter[2] = ((terrain & 0x0000FF00) >> 8);
	letter[3] =  (terrain & 0x000000FF);
		
	for(int i = 0; i < 4; ++i) {
		if(letter[i] != 0) {
			result.push_back(letter[i]);
		} else {
			// no letter, means no more letters at all
			// so leave the loop
			break;
		}
	}

	// make sure the string gets the proper size
	result.resize(7, ' ');
	
	return result;
}

t_letter string_to_builder_number_(std::string str)
{
	//strip the spaces around us
	const std::string& whitespace = " \t";
	str.erase(0, str.find_first_not_of(whitespace));
	if(! str.empty()) {
		str.erase(str.find_last_not_of(whitespace) + 1);
	}

	// empty string is allowed here so handle it
	if(str.empty()) {
		return NONE_TERRAIN;
	}
	
	const int number = lexical_cast_default(str, -1);
	if(number == -1) {
		// at this point we have a single char
		// which should be interpreted by the map
		// builder, so return this number
		return str[0];
	} else {
		wassert(number >= 0 && number < 2^24); 
		return (number << BUILDER_SHIFT);
	}
}
	
} //namespace
