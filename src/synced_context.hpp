
/*
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef SYNCED_CONTEXT_H_INCLUDED
#define SYNCED_CONTEXT_H_INCLUDED

#include <boost/function.hpp>
#include "synced_commands.hpp"
#include "synced_checkup.hpp"
#include "replay.hpp"
#include "random_new.hpp"
#include "random_new_synced.hpp"
#include "generic_event.hpp"
#include <boost/shared_ptr.hpp>
class config;

//only static methods.
class synced_context
{
public:
	enum syced_state {UNSYNCED, SYNCED, LOCAL_CHOICE};

	typedef boost::function1<void, config&> ftype;
	/**
		
		Sets the context to 'synced', initialises random context, and calls the given function.
		The plan is that in replay and in real game the same function is called.
		

		when redoing an action it does not matter whether we run them i synced context because it's save to assume that they do not have any mean effect (otherwise they couldn�t be undone)
		
		TODO: call clear undo stack in the right places (get_user_choice / ask_server) DONE
		


		TODO: implement the "Deterministic SP Mode".
		TODO: implement access to get_syced_state() in lua. DONE.
		TODO: repair checkups in attacks. DONE
		TODO: handle layer leaving  during get_user_choice correctly DONE
		
		
		TODO: move_unit currently ignores when the unit moves further than it can, 
			it would be good to give an oos error especially to notice cheating in mp.
		
		TODO: movement commands are currently treated specially, 
			thats because actions::move_unit returns a value and some function use that value.
			maybe i should add a way here to return a value.
		
		TODO: move_unit tried to preserve the replay at all costs, the disadvantage is 
			1)we notice OOS later, 
			2)we don't notice another player cheating in MP, 
			3)i forgot
			4)we make the code more complicated
			my proposed solution would be to execute the command normally 
			and then just set the unit to the replay destination after we gave an OOS error. 

		TODO: undos are crrently recorded AFTER the action takes place, 
			that means you cannot disallow an action during it's execution by calling undo_stack->clear();
			it would make things easier if that was possible.
		
		TODO: the ai can currently not decide how units advance.
		TODO: don't change comments in this header everytime i do something because that causes rebuild of other files ...

		@param save_in_replay whether data should be saved in replay.
		@param use_undo this parameter is used to ignore undos during an ai move to optimize.
		@param store_in_replay only true if called by do_replay_handle
		@param error_handler an error handler for the case that data contains invalid data.
		
		@return true if the action was successful.

		

	 */
	static bool run_in_synced_context(const std::string& commandname,const config& data, bool use_undo = true, bool show = true ,  bool store_in_replay = true , synced_command::error_handler_function error_handler = default_error_function);
	/*
		Returns whether we are currently executing a synced action like recruit, start, recall, disband, movement, attack, init_side, end_turn, fire_event, lua_ai, auto_shroud or similar.
	*/
	static syced_state get_syced_state();
	/*
		should only be called form set_scontext_synced, set_scontext_local_choice
	*/
	static void set_syced_state(syced_state newstate);
	/*
		Generates a new seed for a synced event, by asking the 'server'
	*/
	static int generate_random_seed();
	/*
		called from get_user_choice; 
	*/
	static void pull_remote_user_input();
	/*
		a function to be passed to run_in_synced_context to assert false on error (the default).
	*/
	static void default_error_function(const std::string& message, bool heavy);
	/*
		a function to be passed to run_in_synced_context to log the error.
	*/
	static void just_log_error_function(const std::string& message, bool heavy);
	/*
		a function to be passed to run_in_synced_context to ignore the error.
	*/
	static void ignore_error_function(const std::string& message, bool heavy);
	/*
		returns a rng_deterministic if in determinsic mode otherwise a rng_synced.
	*/
	static boost::shared_ptr<random_new::rng> get_rng_for(const std::string& commandname);
private:
	/*
		similar to get_user_choice but asks the server instead of a user.
	*/
	static config ask_server(const std::string &name, const mp_sync::user_choice &uch);
	/*
		weather we are in a synced move, in a user_choice, or none of them
	*/
	static syced_state state_;
	/*
		as soon as get_user_choice is used with side != current_side (for example in generate_random_seed) other sides execute the command simultaneously and is_simultaneously is set to true
		is impossible to undo that.
		
		also during the execution of networked turns this should always be true.
		false = we are on a local turn and havent sended anything yet.
		
		this variable is currently not used, the original plan was to use it to send data as soon as possible if is_simultaneously_= true.
		*/
	static bool is_simultaneously_;
	/*
		TODO: replace ai::manager::raise_sync_network with this event because ai::manager::raise_sync_network has nothing to do with ai anymore.
	*/
	static events::generic_event remote_user_input_required_;
};

/*
	a RAII object to enter the synced context, cannot be called if we are already in a synced context.
*/
class set_scontext_synced
{
public:
	set_scontext_synced(const std::string& commanname);
	set_scontext_synced();
	/*
		use this if you have multiple synced_context but only one replay entry.
	*/
	set_scontext_synced(int num);
	~set_scontext_synced();
	int get_random_calls();
private:
	//only called by contructors.
	void init();
	random_new::rng* old_rng_;
	boost::shared_ptr<random_new::rng> new_rng_;
	checkup* old_checkup_;
	synced_checkup new_checkup_;
};

/*
	a RAII object to temporary leave the synced context like in  wesnoth.synchronize_choice. Can only be used from inside a synced context.
*/

class set_scontext_local_choice
{
public:
	set_scontext_local_choice();
	~set_scontext_local_choice();
private:
	random_new::rng* old_rng_;
};

/*
	a helper object to server random seed generation.
*/
class random_seed_choice : public mp_sync::user_choice
{
public: 
	random_seed_choice();
	virtual ~random_seed_choice();
	virtual config query_user() const;
	virtual config random_choice() const;
};


#endif