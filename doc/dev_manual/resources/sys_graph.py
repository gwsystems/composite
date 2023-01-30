#!/bin/python

# Note: This should only be run within the directory of the script. Note the hard-coded paths here.

import glob
import subprocess
import re


dep_script = "../../src/components/cidl/calculate_dependencies.py"
comp_base = "../../src/components/"
components = comp_base + "implementation/*/*/"
libraries = comp_base + "lib/*/"
interfaces = comp_base + "interface/*/"


def filter_out_skel(p):
    s = re.split("/", p.rstrip())
    s = list(filter(lambda e: e != "", s))
    return not (s[-1] == "skel" or s[-2] == "skel") and not (s[-2] == "archives")


cs = filter(filter_out_skel, glob.glob(components))
ls = filter(filter_out_skel, glob.glob(libraries))
ifs = filter(filter_out_skel, glob.glob(interfaces))


def unique(l):
    return list(set(l))


# listify the output
def clean_dep_output(lst):
    if isinstance(lst, bytes):
        lst = lst.decode("utf-8")
    return unique(re.split(" ", lst.rstrip()))


def gather_deps(path):
    libdeps = clean_dep_output(
        subprocess.check_output(["python", dep_script, path, comp_base, "libdeps"])
    )
    ifdeps = clean_dep_output(
        subprocess.check_output(["python", dep_script, path, comp_base, "ifdeps"])
    )
    return filter(lambda d: d != "", unique(libdeps + ifdeps))


def comp_name(p):
    s = re.split("/", p.rstrip())
    s = list(filter(lambda e: e != "", s))
    return s[-2] + "." + s[-1]


def lib_or_if_name(p):
    s = re.split("/", p.rstrip())
    s = list(filter(lambda e: e != "", s))
    return s[-1]


compdeps = {}
libdeps = {}
ifdeps = {}

for c in cs:
    compdeps[comp_name(c)] = gather_deps(c)
for l in ls:
    libdeps[lib_or_if_name(l)] = gather_deps(l)
for i in ifs:
    ifdeps[lib_or_if_name(i)] = gather_deps(i)

header = """digraph composite_software {
label = "Component Software Dependencies" ;
size = "7.5" ;
ratio = "fill" ;
/* rotate = "90" ; */
margin = "0" ;
nodesep = "0.1" ;
ranksep = "0.1" ;
overlap = "false" ;
"""
footer = """fontsize = "56" ;
fontname="Sans serif" ;
}"""

nodes = ""
edges = ""
fontattr = 'fontname="Sans serif",fontsize="48"'

for (c, ds) in compdeps.items():
    nodes += (
        '"'
        + c
        + '" [shape=hexagon,style=filled,fillcolor=lightblue,'
        + fontattr
        + "] ;\n"
    )
for (l, ds) in libdeps.items():
    nodes += (
        '"' + l + '" [shape=oval,style=filled,fillcolor=gray82,' + fontattr + "] ;\n"
    )
for (i, ds) in ifdeps.items():
    nodes += (
        '"'
        + i
        + '" [shape=rectangle,style=filled,fillcolor=lightsteelblue,'
        + fontattr
        + "] ;\n"
    )

for (c, ds) in compdeps.items():
    for d in ds:
        edges += '"' + c + '" -> "' + d + '" ;\n'
for (l, ds) in libdeps.items():
    for d in ds:
        edges += '"' + l + '" -> "' + d + '" ;\n'
for (i, ds) in ifdeps.items():
    for d in ds:
        edges += '"' + i + '" -> "' + d + '" ;\n'


print(header + nodes + edges + footer)
