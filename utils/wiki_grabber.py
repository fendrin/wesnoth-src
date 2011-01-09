#!/usr/bin/env python
# vim: tabstop=4: shiftwidth=4: expandtab: softtabstop=4: autoindent:
# $Id$

"""
   Copyright (C) 2007 - 2009 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.


The wiki grabber is a tool to convert wiki comment formatting[1] into a text
page which can be used in the wiki.

[1] http://wesnoth.org/wiki/Wiki_grabber
"""
from __future__ import with_statement     # For python < 2.6

import operator
import os
import sys
import re

try:
    import argparse
except ImportError:
    print 'Please install argparse by running "easy_install argparse"'
    sys.exit(1)

if __name__ == "__main__":
    # setup and parse command line arguments
	# The defaults are set to the values of the older hardcoded implementation.
    parser = argparse.ArgumentParser(description='The wiki grabber is a tool'
            + ' to convert wiki comment formatting into a text page which can'
            + ' be used in the wiki. For more details, see'
            + ' http://wesnoth.org/wiki/Wiki_grabber')
    parser.add_argument('-s', '--src-dir', default='../src/gui', dest='src_dir',
            help="the location of wesnoth's source code")
    parser.add_argument('-o', '--output', default='/tmp/', dest='output_dir',
            help='the output directory')
    args = parser.parse_args()

    # contains all output generated:
    # - key filename
    # - value node list
    #
    # every node is a list with 2 items
    # - first the sorting order
    # - second the actual data
    file_map = {}

    # contains all macros:
    # - key macro name
    # - value macro contents
    macro_map = {}

    output_directory = args.output_dir
    src_directory = args.src_dir

    if not os.path.isdir(output_directory):
        raise IOError("'%s' isn't a directory." % output_directory)
    if not os.path.isdir(src_directory):
        raise IOError("'%s' isn't a directory." % src_directory)

    # current file being processed
    current_file = ""

    # current block being processed
    current_block = ""

    def is_empty(res, data):
        """
        This checks whether or not a table is empty and writes to stderr if it is.
        It returns True if the table is empty, False otherwise.
        """
        if not res:
            sys.stderr.write("Empty container:\n%s\n" % data)
            return True
        return False


    def reindent(data):
        """Converts the raw input to an easier to use format.

        Lines starting with 8 spaces are concatenated with the previous line.
        The start of line ' *' are removed and if another space exists it's also
        removed."""

        # concatenate
        data = re.sub(r'\n \*(?: ){8,}', " ", data)

        # strip
        data = re.sub(" \*(?: |)", "", data)

        return data

    def get_value(data, key):
        """Extracts data from a key value pair, a key must start at the start of a line."""

        res = re.compile("^" + key + " *= *(.*)$", re.M).search(data)

        if res != None:
            res = res.group(1)

        return res

    def process_header(data):
        """Processes the header."""

        page = get_value(data, "@page")
        order = get_value(data, "@order")
        if order == None:
            order = 10000

        return [page, order]

    def debug_dump(data, res):
        """Show the data the regex retrieved from a match.

        data is the raw data the regex tried to match.
        res is the result of the regex.findall.
        """
        sys.stderr.write("data : %s" % data)
        for i, val in res:
            for j, sub_val in val:
                sys.stderr.write("Line %s match %s: %s\n" % (i, j, sub_val))

    def format(data):
        """Formats the data for the wiki.

        @* -> \n* needed in a list.
        @- -> \n needed to add text after a list."""
        data = re.sub(r'@\*', "\n*", data)
        data = re.sub(r'@\-', "\n", data)
        return data

    def create_config_table(data):
        """Creates a table for data in a config table.

        A config table is a table with info about WML configuration key value pairs.
        """

        # matches a line like
        # x1 & f_unsigned & 0 &            The x coordinate of the
        # startpoint. $
        # x1 & f_unsigned & &              The x coordinate of the
        # startpoint. $
        regex = re.compile("([A-Za-z]\w*) +& +([A-Za-z]\w*) +& +([^&]*?) *& +(.*) +\$")
        res = regex.findall(data)

        if is_empty(res, data):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!key\n!type\n!default\n!description\n"
        for i in res:
            result += "|-\n"
            result += "| %s\n" % i[0]
            result += "| [[GUIVariable#%s|%s]]\n" % (i[1], i[1])
            if not i[2]:
                result += "| mandatory\n"
            else:
                result += "| %s\n" % i[2]
            result += "| %s\n" % format(i[3])
        result += "|}"

        return result

    def create_formula_table(data):
        """Creates a table for data in a formula table.

        A formula table is a table with info about which function parameters
        are available for processing in WML formulas.
        """

        #matches a line like
        #width & unsigned &                 The width of the canvas. $
        regex = re.compile("([A-Za-z]\w*) +& +([A-Za-z]\w*) +& +(.*) +\$")
        res = regex.findall(data)

        if is_empty(res, data):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!Variable\n!type\n!description\n"
        for i in res:
            result += "|-\n"
            result += "| %s\n" % i[0]
            result += "| %s\n" % i[1]
            result += "| %s\n" % format(i[2])
        result += "|}"

        return result

    def create_variable_types_table(data):
        """Creates a table for the variable types."""

        #matches a line like
        # int &                            Signed number (whole
        # numbers). $
        regex = re.compile("([A-Za-z]\w*) +& +(.*) +\$")
        res = regex.findall(data)

        if is_empty(res, data):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!Variable\n!description\n"
        for i in res:
            result += "|-\n"
            result += '| <span id="%s">%s</span>\n' % (i[0], i[0])
            result += "| %s\n" % format(i[1])
        result += "|}"

        return result

    def create_widget_overview_table(data):
        """Creates a table for all available widgets."""
        #matches a line like
        # Button &                       A push button. $
        regex = re.compile("([A-Za-z]\w*) +& +(.*) +\$")
        res = regex.findall(data)

        if is_empty(res, data):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!Section\n!Description\n"
        for i in res:
            result += "|-\n"
            result += '| <span id="%s">' % i[0].lower()
            result += re.sub(r'_', ' ', i[0])
            result += "</span>"
            result += " ([[GUIWidgetDefinitionWML#%s|definition]]" % i[0]
            result += ", [[GUIWidgetInstanceWML#%s|instantiation]])\n" % i[0]
            result += "| %s\n" % format(i[1])
        result += "|}"

        return result

    def create_window_overview_table(data):
        """Creates a table for all available windows."""
        #matches a line like
        # Addon_connect &               The dialog to connect to the addon server $
        regex = re.compile("([A-Za-z]\w*) +& +(.*) +\$")
        res = regex.findall(data)

        if is_empty(res, data):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!Section\n!Description\n"
        for i in res:
            result += "|-\n"
            result += "| " + re.sub(r'_', ' ', i[0])
            result += " ([[GUIWindowDefinitionWML#%s|definition]])\n" % i[0]
            result += "| %s\n" % format(i[1])
        result += "|}"

        return result

    def create_dialog_widgets_table(data):
        """Creates a table for the widgets in a dialog."""

        regex  = """
        \ *(-*)                                 # 0 indention marker
        ((?:[a-z]|[A-Z]|[0-9]|_)+\ |)\ *&       # 1 optional id may start with an underscore
        \ *(.*)\ +&                             # 2 retval
        \ *(.*?)\ +&                            # 3 type
        \ *(m|o)\ +&                            # 4 mandatory flag
        \ *(.*)\ +\$                            # 5 description
        """

        res = re.compile(regex, re.VERBOSE).findall(data)

        if is_empty(res, data):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!ID (return value)\n!Type\n!Mandatory\n!Description\n"
        for i in res:
            result += "|-\n| " + "&nbsp;" * len(i[0]) * 8

            if not i[1]:
                result += "''free to choose''"
            else:
                result += i[1]

            if not i[2]:
                result += "\n"
            else:
                result += " (%s)\n" % i[2]

            result += "| [[GUIToolkitWML#%s|%s]]\n" % (i[3], i[3])

            if i[4] == 'm':
                result += "| yes\n"
            else:
                result += "| no\n"

            result += "| %s\n" % format(i[5])

        result += "|}"

        return result

    def create_table(table):
        """Wrapper for creating tables."""

        type = table.group(1)

        functions = {
        "config": create_config_table,
        "formula": create_formula_table,
        "variable_types": create_variable_types_table,
        "widget_overview": create_widget_overview_table,
        "window_overview": create_window_overview_table,
        "dialog_widgets": create_dialog_widgets_table}

        try:
            return functions[type](table.group(2) + "\n")
        except KeyError:
            sys.stderr.write("Unknown table definition '%s'.\n" % type)
            return "Unknown table definition '%s'." % type

    def create_wml_reference_description(data):
        # Matches a line like:
        # name & string & "" & 1.5 & description
        # [relative] & node & * & & description

        regex = re.compile("(.*) +& +(.*) +& *(.*) *& +(.*) *& *(.*) +\$")
        res = regex.findall(data)

        if is_empty(res, data):
            return "Empty description"

        result = ''

        for i in res:
           # We don't strip i[0] because it allows to take care of
           # nested items that are of the same type and that are 4
           # spaces indented.
           result += "%s '''''%s''''' " % ("*" * (i[0].rstrip().count(' ') / 4 + 1), i[0].strip())
           if i[1] != 'node':
               result += "(%s, ''%s'') " % (i[1], "default " + i[2].rstrip() if i[2] else "'mandatory'")
           else:
               result += "(%s) " % {'*': "zero or more times", '+': "one or more times", '?': "zero or one times", '1': "one time", '': "empty string bug."}[i[2].rstrip()]
           result += "%s" % i[4]
           if i[3]:
               result += " {{DevFeature%s}}" % i[3].replace(' ', '')
           result += "\n"
        return result

    def create_description(description):
        """Wrapper for creating descriptions."""
        type = description.group(1)

        descriptions = {
            "wml_reference": create_wml_reference_description}

        try:
            return descriptions[type](description.group(2) + "\n")
        except KeyError:
            sys.stderr.write("Unknown description definition '%s' .\n" % type)
            return "Unknown description definition '%s'." % type

    def process_body(data):
        """Process the body.

        The body needs to be stripped of known markup values.
        """


        table_regex = re.compile("^@begin{table}\{(.*?)\}\n(.*?)\n@end{table}$", re.M | re.S)
        data = table_regex.sub(lambda match: create_table(match), data)

        description_regex = re.compile("^@begin{description}\{(.*?)\}\n(.*?)\n@end{description}$", re.M | re.S)
        data = description_regex.sub(lambda match: create_description(match), data)
        return data

    def process(data):
        """Processes a wiki block."""

        data = replace_macros(data)

        res = re.compile("(.*?)\n\n(.*)", re.S).findall(data)
        if res:
            header = process_header(res[0][0])
            body = process_body(res[0][1])
        else:
            print "File: " + current_file
            print "Block:\n" + current_block
            print "\n\nInvalid wiki block, discarded."
            return

        if not header[0]:
            print "File: " + current_file
            print "Block:\n" + current_block
            print "\n\nNo page defined, dropped."
            return

        if not file_map.has_key(header[0]):
            file_map[header[0]] = []

        file_map[header[0]].append([header[1], body])

    def create_output():
        """Generates the output"""

        for file, data_list in file_map.iteritems():
            data_list.sort(key=operator.itemgetter(0))
            with open(os.path.join(output_directory, file), "w") as fd:
                for i in data_list:
                    fd.write(i[1])

    def process_file(name):
        """Processes all wiki blocks (if any) of a file."""

        global current_file
        current_file = name
        with open(name, "r") as file:
            data = file.read()

        regex = re.compile("(/\*WIKI($.*?)^ \*/)", re.M | re.S)
        res = regex.findall(data)
        if res:
            for i in res:
                global current_block
                current_block = i[0]
                section = reindent(i[1])
                wiki_info = process(section)

    def process_directory(dir):
        """Processes a directory.

        Recurses into the sub directory, ignoring hidden directories.
        Processes every .[c|h]pp file.
        """

        items = os.listdir(dir)

        for item in items:

            # Ignore hidden directories.
            if item.startswith("."):
                continue

            if os.path.isdir(os.path.join(dir, item)):
                process_directory(os.path.join(dir, item))
            elif item.endswith(".cpp") or item.endswith(".hpp"):
                process_file(os.path.join(dir, item))

    ##### ##### ##### MACRO PROCESSING ##### ##### #####

    def instanciate_macro(macro):
        """Replaces a macro text by the marco value."""

        global macro_map

        if not macro_map.has_key(macro.group(1)):
            print "Macro '%s' is not defined." % macro.group(1)
            return macro.group(0)

        return macro_map[macro.group(1)]


    def replace_macros(data):
        """Replaces all macros found in the data.

        If a macro isn't defined, the text is left as is."""

        macro_regex = re.compile("@macro *= *(\w+)", re.M | re.S)
        data = macro_regex.sub(lambda match: instanciate_macro(match), data)
        return data

    def create_macro(macro):
        """Wrapper for creating macros."""

        global macro_map

        if macro_map.has_key(macro.group(1)):
            print "Macro '%s' is being redefined." % macro.group(1)

        macro_map[macro.group(1)] = macro.group(2)

    def process_file_macros(name):
        """Processes all wiki macro blocks (if any) of a file."""

        global current_file
        current_file = name
        with open(name, "r") as file:
            data = file.read()

        regex = re.compile("(/\*WIKI_MACRO($.*?)^ \*/)", re.M | re.S)
        res = regex.findall(data)
        if res:
            for i in res:
                global current_block
                current_block = i[0]
                section = reindent(i[1])
                macro_regex = re.compile("^@start_macro *= *(.*?)\n(.*?)\n@end_macro.*?$", re.M | re.S)
                macro_regex.sub(lambda match: create_macro(match), section)


    def process_directory_macros(dir):
        """Processes a directory looking for macros.

        Recurses into the sub directory, ignoring hidden directories.
        Processes every .[c|h]pp file.
        """

        for item in os.listdir(dir):

            # Ignore hidden directories.
            if item.startswith("."):
                continue

            if os.path.isdir(os.path.join(dir, item)):
                process_directory_macros(os.path.join(dir, item))
            elif item.endswith(".cpp") or item.endswith(".hpp"):
                process_file_macros(os.path.join(dir, item))

    ##### ##### ##### MAIN ##### ##### #####


    process_directory_macros(src_directory)
    process_directory(src_directory)

    create_output()
