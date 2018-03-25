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


FNDEP_FILE=".fn_dependencies"
FNEXP_FILE=".fn_exports"
IFDIR="../../../interface/"

path = os.getcwd() #string.rstrip(os.popen("pwd").readlines()[0])
comp = re.split("/", path)[-1]
cif  = re.split("/", path)[-2]

f = open('Makefile', 'r')
lines = f.readlines()
fnexps = []
fnxform = []
for line in lines:
    line = string.rstrip(line)
    data = re.split("=", line)
    if data[0] == "DEPENDENCIES":
       deps = re.split(" ", data[1])
       #deps.append("stkmgr")
       for d in deps:
       	   if d == "":
	      continue
       	   ifpath = IFDIR + d + "/" + FNEXP_FILE
	   try:
		f = open(ifpath, 'r')
	   except IOError:
		print "Warning: component " + cif + "." + comp + " has stated dependency on " + d + " and the interface doesn't exist."
		print "\tSuggested Fix: remove it from the DEPENDENCIES list in components/implementation/" + cif + "/" + comp + "/Makefile"
		continue
	   ls = f.readlines()
	   for l in ls:
	       fnexps.append(string.rstrip(l))
    if data[0] == "FN_PREPEND":
       fnxform = re.split(" ", string.rstrip(data[1]))

f = open(FNDEP_FILE, 'r')
fndeps = []
lines = f.readlines()
for line in lines:
    line = string.rstrip(line)
    fndeps.append(line)

if len(fnxform) != 0:
   nfndeps = []
   for d in fndeps:
       for p in fnxform:
       	   if d.startswith(p):
	      d = d.replace(p, "", 1)
	      break
       nfndeps.append(d)
   fndeps = nfndeps

dangling = []
for undef in fndeps:
    found = 0
    for exp in fnexps:
        if undef + "_rets_inv" == exp or undef + "_inv" == exp:
           found = 1
           break
    if found == 0:
       dangling.append(undef)

if dangling != []:
   print "Error: component " + cif + "." + comp + " does not have stated dependencies to provide " + str(dangling)
   print "\tSuggested Fix: add the proper interface dependency in the DEPENDENCIES list in components/implementation/" + cif + "/" + comp + "/Makefile."
   print "Less likely fixes: a) you should specify a FN_PREPEND in the Makefile for a component that wishes to invoke another component with the same interface."
   print "b) you should reorder dependencies within the Makefile (see FAQ)."
