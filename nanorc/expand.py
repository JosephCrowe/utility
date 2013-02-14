#!/usr/bin/env python2
import sys
import re
import os.path
from collections import OrderedDict

if len(sys.argv) >= 2:
    filename = sys.argv[1]
else:
    print 'Usage: %s FILENAME' % os.path.basename(sys.argv[0])
    sys.exit(1)

def include(name, parents=[]):
    parents = parents + [name]
    with open(name) as file: data = file.read()
    def repl(match):
        (child,) = match.groups()
        if child in parents:
            print 'Error: #include cycle: ' + ' <- '.join(parents + [child])
            sys.exit(1)
        return include(child, parents)
    return re.sub(r'^#include (.*)\r?\n?', repl, data, flags=re.M)

data = include(filename)

defns = OrderedDict()

def define_repl(match):
    name, value = match.groups()
    defns[name] = value.strip()
    return ''
data = re.sub(r'^#define (\w+) (.+)(?:\r?\n?)+', define_repl, data, flags=re.M)

def expand(name, parents=[]):
    parents = parents + [name]
    def repl(match):
        (child,) = match.groups()
        if child not in defns:
            print 'Error: %s is not defined: ' % child + defns[name]
            sys.exit(1)
        if child in parents:
            print 'Error: #define cycle: ' + ' <- '.join(parents + [child])
            sys.exit(1)
        return expand(child, parents)
    defns[name] = re.sub(r'%(\w+)%', repl, defns[name])
    return defns[name]

map(expand, defns.iterkeys())

if '--debug' in sys.argv:
    width = max(len(key) for key in defns.iterkeys())
    for name, value in defns.iteritems():
        print '  %s%s := %s' % (name, ' '*(width - len(name)), value)
    sys.exit(1)

def expand_repl(match):
    (name,) = match.groups()
    if (name not in defns):
        print 'Error: %s is not defined.'
        sys.exit(1)
    return defns[name]
    
data = re.sub(r'%(\w+)%', expand_repl, data)

sys.stdout.write(data)
