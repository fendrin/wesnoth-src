/*
   Copyright (C) 2013 by Pierre Talbot <ptalbot@mopong.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef SERVER_PROTOCOL_ECHO_HPP
#define SERVER_PROTOCOL_ECHO_HPP

#include <string>
#include <array>
#include <vector>
#include <boost/asio.hpp>

class echo_protocol
{
public:
   class reply
   {
      std::string request_data;
   public:
      reply(std::iostream& raw_data_stream)
      {
         raw_data_stream >> request_data;
      }

      std::vector<boost::asio::const_buffer> to_buffers() const
      {
         std::vector<boost::asio::const_buffer> buffers;
         buffers.push_back(boost::asio::buffer(request_data));
         return buffers;
      }
   };
   typedef reply reply_type;

   reply_type handle_request(std::iostream& raw_request_stream) const
   {
      return reply_type(raw_request_stream);
   }
};

#endif // SERVER_PROTOCOL_ECHO_HPP
