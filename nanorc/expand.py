#!/usr/bin/env python2
import sys
import re
import os.path
try:
    from collections import OrderedDict
except ImportError:
    from ordereddict import OrderedDict

if len(sys.argv) >= 2:
    filename = sys.argv[1]
else:
    print 'Usage: %s FILENAME' % os.path.basename(sys.argv[0])
    sys.exit(1)

def stderr(msg):
    sys.stderr.write(msg + '\n')

def include(name, parents=[]):
    parents = parents + [name]
    with open(name) as file: data = file.read()
    def repl(match):
        (child,) = match.groups()
        if child in parents:
            stderr('Error: #include cycle: ' + ' <- '.join(parents + [child]))
            sys.exit(1)
        return include(child, parents)
    return re.compile(r'^#include (.*)\r?\n?', re.M).sub(repl, data)

data = include(filename)

defns = OrderedDict()

def define_repl(match):
    name, value = match.groups()
    defns[name] = value.strip()
    return ''
data = re.compile(r'^##.*', re.M).sub('', data)
data = re.compile(r'^#define (\w+) (.+)(?:\r?\n?)+', re.M).sub(define_repl, data)

def expand(name, parents=[]):
    parents = parents + [name]
    def repl(match):
        (child,) = match.groups()
        if child not in defns:
            stderr('Error: %s is not defined: ' % child + defns[name])
            sys.exit(1)
        if child in parents:
            stderr('Error: #define cycle: ' + ' <- '.join(parents + [child]))
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
        stderr('Error: %s is not defined.' % name)
        sys.exit(1)
    return defns[name]
    
data = re.sub(r'%(\w+)%', expand_repl, data)

sys.stdout.write(data)
