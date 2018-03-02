#!/usr/bin/python

import re
import sys
import os
import math
import string

"""
Generate the .exported_fns file for a component based on its
directory, and its listed INTERFACES.  Also check to make sure that we
implement all the functions defined in our interfaces.
"""

EXPORTEDFN_FILE=".exported_fns"
IFEXP_FILE=".fn_exports"
IFDIR="../../../interface/"

pathl = re.split("/", os.getcwd())
comp  = pathl[-1]
cif   = pathl[-2]

f = open('Makefile', 'r')
lines = f.readlines()
ifexps = []
for line in lines:
    line = string.rstrip(line)
    data = re.split("=", line)
    if data[0] == "INTERFACES":
       ifs = re.split(" ", data[1])
       for i in ifs:
       	   if i == "":
	      continue
	   ifexps.append(i)
       break
if (cif != "no_interface" and "tests" not in cif):
   ifexps.append(cif)
f.close()

iffnexps = []
for inf in ifexps:
    try:
	i = open(IFDIR + inf + "/" + IFEXP_FILE)
    except IOError:
	sys.stderr.write("Warning: component " + cif + "." + comp + " has stated it implements interface " + inf + " and the interface doesn't exist.\n")
	sys.stderr.write("\tSuggested Fix: remove it from the INTERFACES list in components/implementation/" + cif + "/" + comp + "/Makefile\n")
	continue
    ls = i.readlines()
    iffnexps = iffnexps + ls
iffnexps = list(set([string.rstrip(x) for x in iffnexps])) # remove duplicates
iffnexps = [string.replace(x,"_inv", "") for x in iffnexps]

f = os.popen("nm --extern-only c.o | awk '{if (NF == 3 && ($2 == \"T\" || $2 == \"W\")) {print $3}}'")
fns = f.readlines()
fns = [string.rstrip(x) for x in fns]

iffnexpsrets = []
for iffn in iffnexps:
    if (iffn not in fns):
       iffnexpsrets.append(iffn)
    else:
       print(iffn)

iffnexpsrets = [string.replace(x,"_rets", "") for x in iffnexpsrets]
for iffn in iffnexpsrets:
    if (iffn not in fns):
       sys.stderr.write("Warning: in component " + cif + "." + comp + ", function " + iffn + " defined in an interface, but not in component\n")
    else:
       print(iffn)
