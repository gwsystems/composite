#!/usr/bin/python

import re
import sys
import os
import math
import string

"""
The .fn_dependencies file in the component's directory includes a list
of the functions that component has that are undefined.  The
.fn_exports file is in each interface directory and includes the
functions that are exported by that interface.  The Makefile for each
component includes two lines of note: one that includes
DEPENDENCIES=dlist and one that includes FN_PREPEND=plist where dlist
is a list of the interfaces the component is dependent on, and pist is
the list of function prepends the component uses.  These function
prepends are for an edge case: What happens when a component that
exports interface sched (for instance) also has dependencies on sched?
C will compile any calls intended for the "parent" into recursive
self-calls.  So Composite allows you to prepend a string onto each
function call that is meant to go to the parent.  The plist includes
these prepended strings that must be used to match up the undefined
function call to the actual exported function in an interface.

All of the checking done here is also done in the system loader, but
we should really be doing this at compile time, thus this functional
dependency completeness check.

TODO: also check that we export all of the function for the interfaces
we implement.
"""

UNDEF_CMD="nm -u c.o | awk '{print $2}'"

FNDEP_FILE=".fn_dependencies"
FNEXP_FILE=".fn_exports"
IFDIR="../../../interface/"

path = os.getcwd() #string.rstrip(os.popen("pwd").readlines()[0])
comp = re.split("/", path)[-1]
cif  = re.split("/", path)[-2]

f = os.popen(UNDEF_CMD)
fndeps = f.readlines()
nfndeps = []
for d in fndeps:
    nfndeps.append(string.rstrip(d))
fndeps = nfndeps

f = open('Makefile', 'r')
lines = f.readlines()
fnxform = []
for line in lines:
    line = string.rstrip(line)
    data = re.split("=", line)
    if data[0] == "FN_PREPEND":
       fnxform = re.split(" ", string.rstrip(data[1]))

if len(fnxform) != 0:
   nfndeps = []
   for d in fndeps:
       for p in fnxform:
       	   if d.startswith(p):
	      d = d.replace(p, "", 1)
	      break
       nfndeps.append(d)
   fndeps = nfndeps

for d in fndeps:
    print d
