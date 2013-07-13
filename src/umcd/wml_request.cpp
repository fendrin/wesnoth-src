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
#include "umcd/wml_request.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

wml_request::wml_request(network_stream& raw_data_stream, const validator_ptr& validator)
: stream(raw_data_stream)
{
   ::read(data.get_metadata(), stream, validator.get());
   UMCD_LOG_IP(debug, stream) << " -- request validated.\n";
}

network_data& wml_request::get_data() 
{ 
   return data; 
}

std::string wml_request::name() const
{
   config::all_children_iterator iter = data.get_metadata().ordered_begin();
   if(iter == data.get_metadata().ordered_end())
     return "";
   return iter->key;
}

wml_request::network_stream& wml_request::get_stream()
{
   return stream;
}

static void check_stream_state(std::istream& raw_data_stream, std::string error_msg)
{
   if(!raw_data_stream.good())
   {
      throw std::runtime_error(error_msg);
   }
}

std::string peek_request_name(std::istream& raw_data_stream)
{
   // Try to read the first tag which is the name of the packet.
   std::string error_msg("Invalid packet. The request name could not have been read.");
   char first_bracket;
   raw_data_stream >> first_bracket;
   check_stream_state(raw_data_stream, error_msg);
   if(first_bracket != '[')
      throw std::runtime_error(error_msg);
   std::string request_name;
   getline(raw_data_stream, request_name, ']');
   check_stream_state(raw_data_stream, error_msg);

   // Put back name in the stream. So the parsing of the config will be ok.
   raw_data_stream.putback(']');
   std::string::const_reverse_iterator b = request_name.rbegin();
   std::string::const_reverse_iterator e = request_name.rend();
   for(; b != e; ++b)
      raw_data_stream.putback(*b);
   raw_data_stream.putback(first_bracket);

   return request_name;
}

wml_request make_request(boost::asio::ip::tcp::iostream& raw_data_stream, const std::string& validator_file_path)
{
   using namespace schema_validation;
   return wml_request(raw_data_stream, boost::make_shared<one_hierarchy_validator>(validator_file_path));
}