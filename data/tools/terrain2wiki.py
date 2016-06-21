#!/usr/bin/python
# encoding: utf-8

"""
A script to create the "Terrain Table" on the TerrainCodeTableWML wiki page.
Add the output to the wiki whenever a new terrain is added to mainline.
"""

from __future__ import with_statement   # For python < 2.6
import os
import sys
import re
try:
    import argparse
except ImportError:
    print('Please install argparse by running "easy_install argparse"')
    sys.exit(1)

# Where to get terrain images
terrain_url = "https://raw.github.com/wesnoth/wesnoth/master/data/core/images/terrain/%s.png"


def parse_terrain(data):
    """
    Parses the terrains. Input looks like this:

    [terrain_type]
    symbol_image=water/ocean-grey-tile
    id=deep_water_gray
    editor_name= _ "Gray Deep Water"
    string=Wog
    aliasof=Wo
    submerge=0.5
    editor_group=water
    [/terrain_type]

    Output is a text in wiki format.
    """

    # Remove all comments.
    data = "\n".join([i for i in data.split("\n") if not re.match("^\s*#", i)])
    terrains = re.compile(r"\[terrain_type\](.*?)\[\/terrain_type\]", re.DOTALL).findall(data)

    data = """{{AutogeneratedWML}}{| border="1"
!terrain
!name
!string
!alias of
!editor group
"""

    for i in terrains:
        # Strip unneeded things.
        i = i[5:]
        i = i.split("\n    ")
        # Don't parse special files that are hacks. They shouldn't be used
        # directly. (They're only there to make aliasing work.)
        if i[0].startswith(" "):
            continue
        # This avoids problems due to additional = in strings. Exact string
        # removal does not matter as long as we do not print help_topic_text
        # in the wiki page.
        removeus = ("<italic>text='", "'</italic>", "<ref>dst='", "text='", "'</ref>")
        for text in removeus:
            i = [a.replace(text, "") for a in i]
        # Create a dictionary of key and values
        content = dict([v.strip().split("=") for v in i])
        # Hidden things shouldn't be displayed
        if 'hidden' in content:
            continue
        data += """|-
| %s
| %s
| <code>%s</code>
| <code>%s</code>
| %s
""" % (
terrain_url % (content['editor_image'] if 'editor_image' in content else content['symbol_image']),
content['editor_name'][4:-1] if 'editor_name' in content else content['name'][4:-1],
content['string'].replace("# wmllint: ignore", "").replace("|", "&#124;"),
content['aliasof'].replace("|", "&#124;") if 'aliasof' in content else "",
content['editor_group'])

    data += "|}"
    return data

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='terrain2wiki is a tool to\
convert the terrain codes located in terrain.cfg to wiki formatted text.')
    parser.add_argument('-f', '--file', default='data/core/terrain.cfg',
dest='path', help="The location of the terrain.cfg file.")
    parser.add_argument('-o', '--output', default='/tmp/TerrainCodeTableWML',
dest='output_path', help="The location of the output file.")
    args = parser.parse_args()

    path = args.path
    output_path = args.output_path

    if not os.path.exists(path) or not path.endswith('.cfg'):
        print("Invalid path: '%s' does not exist or not a .cfg file.") % path
        sys.exit(1)

    with open(path, "r") as input_file:
        data = input_file.read()
    data = parse_terrain(data)

    with open(output_path, "w") as output:
        output.write(data)
