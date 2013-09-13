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



#include "umcd/server_options.hpp"
#include "umcd/server/multi_threaded/server_mt.hpp"
#include "umcd/server/daemon.hpp"
#include "umcd/protocol/server/entry_point.hpp"
#include "umcd/logger/asio_logger.hpp"
#include "umcd/env/environment_loader.hpp"
#include "umcd/env/database_info.hpp"
#include "umcd/env/protocol_info.hpp"
#include "umcd/env/server_core.hpp"
#include "umcd/protocol/core/header_mutable_buffer.hpp"
#include "umcd/database/database.hpp"

#include "wml_exception.hpp"
#include "config.hpp"

#include "umcd/boost/thread/workaround.hpp"
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>

#include <stdexcept>

using namespace umcd;

static void endpoint_failure_handler(const std::string& message)
{
	UMCD_LOG(info) << message << "\n";
}

static void start_success_handler(const boost::asio::ip::tcp::endpoint& message)
{
	UMCD_LOG(info) << "Server launched (" << message << ").";
}

static void start_failure_handler()
{
	throw std::runtime_error("No endpoints found - Check the status of your network interfaces.\n");
}

static void on_run_exception_handler(const std::exception& e)
{
	UMCD_LOG(error) << "Exception in basic_server::run(): handler shouldn't launch exception! (" << e.what() << ").";
}

static void on_run_unknown_exception_handler()
{
	UMCD_LOG(error) << "Exception in basic_server::run(): handler shouldn't launch exception! (this exception doesn't inherit from std::exception).";
}

int main(int argc, char *argv[])
{
	bool logger_initialized = false;
	try
	{
		server_options options(argc, argv);
		if(options.has_required_options())
		{
			config cfg = options.read_config();

			protocol_info info;
			info.on_header_max_size_change(umcd::core::header_mutable_buffer::set_header_max_size);

			environment_loader env_loader;
			env_loader.load(cfg);

			// Badly the environment must me loaded before we can validate.
			// (Because the game_config::path must be initialized to validate).
			options.validate(cfg);

			asio_logger::get().load(logging_info());
			logger_initialized = true;

			database::init_db(database_info());

			if(options.is_daemon())
			{
				boost::optional<std::string> err = launch_daemon();
				if(err)
				{
					UMCD_LOG(error) << *err;
					UMCD_LOG(warning) << "The server has been launched in frontend mode.";
				}
			}

			server_core core;
			server_mt addon_server(core.threads());
			// Set server event handlers.
			addon_server.on_event<endpoint_failure>(endpoint_failure_handler);
			addon_server.on_event<start_success>(start_success_handler);
			addon_server.on_event<start_failure>(start_failure_handler);
			addon_server.on_event<on_run_exception>(on_run_exception_handler);
			addon_server.on_event<on_run_unknown_exception>(on_run_unknown_exception_handler);
			addon_server.on_event<on_new_client>(protocol_entry_point);

			// Launch logger.
			asio_logger logger(boost::ref(addon_server.get_io_service()), boost::posix_time::milliseconds(500));

			addon_server.start(core.port());
			UMCD_LOG(info) << addon_server.thread_pool_size() << " cores found.";
			addon_server.run();
		}
	}
	catch(const twml_exception& e)
	{
		UMCD_LOG(fatal) << " (user message=" << e.user_message << " ; dev message=" << e.dev_message << ")";
	}
	catch(std::exception &e)
	{
		UMCD_LOG(fatal) << e.what();
	}
	if(logger_initialized)
		RUN_ONCE_LOGGER();
	return 0;
}
