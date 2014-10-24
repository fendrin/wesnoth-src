/*
   Copyright (C) 2003 - 2014 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GETTEXT_HPP_INCLUDED
#define GETTEXT_HPP_INCLUDED

/**
 * How to use gettext for wesnoth source files:
 * -# include this header file in the .cpp file
 * -# make sure, that the source file is listed in the respective POTFILES.in
 *    for the textdomain, in the case of wesnoth-lib it is this file:
 *    po/wesnoth-lib/POTFILES.in
 * -# add the following include to set the correct textdomain, in this example
 *    wesnoth-lib (not required for the domain 'wesnoth', required for all
 *    other textdomains).
 *    @code
 *    #define GETTEXT_DOMAIN "wesnoth-lib"
 *    @endcode
 *
 * This should be all that is required to have your strings that are marked
 * translatable in the po files and translated ingame. So you at least have
 * to mark the strings translatable, too. ;)
 */

// gettext-related declarations
#include "wesconfig.h"

//A Hack to make the eclipse-cdt parser happy.
#ifndef GETTEXT_DOMAIN
# define GETTEXT_DOMAIN PACKAGE
#endif

#ifdef __CDT_PARSER__
# define GETTEXT_DOMAIN ""
#endif

namespace translation 
{
	const char* dgettext(const char* domain, const char* msgid);
	const char* egettext(const char*);
	const char* dsgettext(const char * domainname, const char *msgid);
	//const char* sngettext(const char *singular, const char *plural, int n);
	const char* dsngettext(const char * domainname, const char *singular, const char *plural, int n);
	
	static const char* gettext(const char* str) 
	{ return translation::dgettext(GETTEXT_DOMAIN, str); }
	static const char* sgettext(const char* str) 
	{ return translation::dsgettext(GETTEXT_DOMAIN, str); }
	static const char* sngettext(const char* str1, const char* str2, int n) 
	{ return translation::dsngettext(GETTEXT_DOMAIN, str1, str2 , n); }

	void bind_textdomain(const char* domain, const char* direcory, const char* encoding);
	void set_default_textdomain(const char* domain);
}

#define _(String) translation::dsgettext(GETTEXT_DOMAIN,String)
#define _n(String1, String2, Int) translation::dsngettext(GETTEXT_DOMAIN, String1,String2,Int)

#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#endif
