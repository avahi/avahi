#!/usr/bin/python

import os, sys

def usage(ret = 0):
    print "%s [-s] <file> <symbol>" % sys.argv[0]
    sys.exit(ret)

args = sys.argv[1:]

use_static = False

if len(args) >= 1 and args[0] == '-s':
    use_static = True
    args = args[1:]

if len(args) >= 1 and args[0] == '-h':
    usage()

if len(args) != 2:
    sys.stderr("Wrong number of arguments")
    usage(1)

f = file(args[0])
t = f.read()
f.close()

out = sys.stdout

if use_static:
    out.write("static ")

out.write('const char %s[] = \n"' % args[1]);

n = 0

for c in t:
    if c == '\n':
        out.write('\\n"\n"')
        n = 0
    elif c == '"':
        out.write('\\"')
        n += 2
    elif ord(c) < 32 or ord(c) >= 127:
        out.write('\\x%02x' % ord(c))
        n += 4
    else:
        out.write(c)
        n += 1

    if n >= 76:
        out.write('"\n"')
        n = 0
        
out.write('";\n');

