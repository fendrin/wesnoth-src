/* $Id$ */
/*
   Copyright (C) 2009 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef SERVER_ROOM_HPP_INCLUDED
#define SERVER_ROOM_HPP_INCLUDED

#include "../network.hpp"
#include "player.hpp"
#include "simple_wml.hpp"

namespace wesnothd {

typedef std::vector<network::connection> connection_vector;

class game;

/**
 * A room is a group of players that can communicate via messages.
 */
class room {
public:
	/**
	 * Construct a room
	 */
	room();

	/**
	 * Return the number of players in this room
	 */
	size_t size() const {
		return members_.size();
	}

	/**
	 * Return the members of this room
	 */
	const std::vector<network::connection>& members() const {
		return members_;
	}

	/**
	 * Membership checker.
	 * @return true iif player is a member of this room
	 */
	bool is_member(network::connection player) const {
		return std::find(members_.begin(), members_.end(), player) != members_.end();
	}

	/**
	 * Joining the room
	 * @return true if the player was succesfully added
	 */
	bool add_player(network::connection player);

	/**
	 * Add all players from a game into this room
	 */
	void add_players(const game& game);

	/**
	 * Leaving the room
	 */
	void remove_player(network::connection player);

	/**
	 * Chat message processing
	 */
	void process_message(simple_wml::document& data, const player_map::iterator user);

	/**
	 * Convenience function for sending a wml document to all (or all except
	 * one) members.
	 * @see send_to_many
	 * @param data        the document to send
	 * @param exclude     if nonzero, do not send to this player
	 * @param packet_type the packet type, if empty the root node name is used
	 */
	void send_data(simple_wml::document& data, const network::connection exclude=0, std::string packet_type = "") const;

	/**
	 * Send a text message to all members
	 * @param message the message text
	 * @param exclude if nonzero, do not send to this player
	 */
	void send_server_message_to_all(const char* message, network::connection exclude=0) const;

	/**
	 * Prepare a text message and/or send it to a player. If a nonzero sock
	 * is passed, the message is sent to this player. If a non-null pointer
	 * to a simple_wml::document is passed, the message is stored in that
	 * document.
	 * @param message the message text
	 * @param sock    the socket to send the message to, if nonzero
	 * @param docptr  the wml document to store the message in, if nonnull
	 */
	void send_server_message(const char* message, network::connection sock,
		simple_wml::document* docptr = NULL) const;

private:
	connection_vector members_;
};

} //end namespace wesnothd

#endif
