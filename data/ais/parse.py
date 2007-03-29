import re, os, safe

whitelisted = ["wesnoth", "heapq", "random"]
rex = re.compile(r"^import\s+(.*)", re.M)

def include(matchob):
    """
    Regular expression callback. Handles a single import statement, returning
    the included code.
    """
    names = [x.strip() for x in matchob.group(1).split(",")]
    r = ""
    for name in names:
        if name in whitelisted: continue
        for path in pathes:
            includefile = os.path.join(path, name)
            try:
                code = parse_file(includefile + ".py")
                break
            except IOError:
                pass
        else:
            raise safe.SafeException("Could not include %s." % name)
            return None
        r += code
    return r

def parse_file(name):
    """
    Simple pre-parsing of scripts, all it does is allow importing other scripts.
    """
    abspath = os.path.abspath(name)
    if abspath in already: return ""
    already[abspath] = 1
    code = file(abspath).read().replace(chr(13), "")
    code = rex.sub(include, code)
    return code

def parse(name):
    global already
    already = {}
    return parse_file(name)

