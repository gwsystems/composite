#!/bin/python

# Generate the per-interface, library, and component documentation.

import glob
import subprocess
import re
import string
import tempfile
import os
import copy

dep_script = "../../src/components/cidl/calculate_dependencies.py"
comp_base  = "../../src/components/"
components = comp_base + "implementation/*/*/doc.md"
libraries  = comp_base + "lib/*/doc.md"
interfaces = comp_base + "interface/*/doc.md"

def readfile(p):
    with open(p) as f:
        return f.read()
    return ""

def remove_trailing_doc(wildcard_path):
    ps = glob.glob(wildcard_path)
    removed = []
    for p in ps:
        # remove the "doc.md" from the path
        s = re.split("/", string.rstrip(p))
        pathified = "/".join(s[0:-1]) + "/"
        removed.append(pathified)
    return removed

def filter_out_skel(p):
    s = re.split("/", string.rstrip(p))
    s = filter(lambda e: e != '', s)
    return not(s[-1] == "skel" or s[-2] == "skel") and not(s[-2] == "archives")

cs  = filter(filter_out_skel, remove_trailing_doc(components))
ls  = filter(filter_out_skel, remove_trailing_doc(libraries))
ifs = filter(filter_out_skel, remove_trailing_doc(interfaces))

def unique(l):
    return list(set(l))

# listify the output
def clean_dep_output(lst):
    return filter(lambda d: d != '', unique(re.split(" ", string.rstrip(lst))))

def gather_deps(path):
    libdeps = clean_dep_output(subprocess.check_output(['python', dep_script, path, comp_base, 'shallowlibdeps']))
    ifdeps  = clean_dep_output(subprocess.check_output(['python', dep_script, path, comp_base, 'shallowifdeps']))
    ifexps  = clean_dep_output(subprocess.check_output(['python', dep_script, path, comp_base, 'shallowifexps']))
    return (libdeps, ifdeps, ifexps)

def comp_name(p):
    s = re.split("/", string.rstrip(p))
    s = filter(lambda e: e != '', s)
    return s[-2] + "." + s[-1]

def lib_or_if_name(p):
    s = re.split("/", string.rstrip(p))
    s = filter(lambda e: e != '', s)
    return s[-1]

compdeps = {}
libdeps  = {}
ifdeps   = {}

for c in cs:
    compdeps[comp_name(c)] = { "deps": gather_deps(c), "type": "component", "path":c }
for l in ls:
    libdeps[lib_or_if_name(l)] = { "deps": gather_deps(l), "type": "library", "path":l }
for i in ifs:
    ifdeps[lib_or_if_name(i)] = { "deps": gather_deps(i), "type": "interface", "path":i }

header = """digraph blob_dependencies {
/* label = "Component Software Dependencies" ; */
/* size = "7" ;*/
/* ratio = "fill" ; */
/* rotate = "90" ; */
margin = "0" ;
/* nodesep = "0.1" ; */
/* ranksep = "0.1" ; */
overlap = "false" ;
fontname="Sans serif" ;

"""
footer = "}"

def gen_node(n, typestr):
    fontattr = "fontname=\"Sans serif\""
    if   typestr == "component":
        return "\"" + n + "\" [shape=hexagon,style=filled,fillcolor=lightblue," + fontattr + "] ;\n"
    elif typestr == "library":
        return "\"" + n + "\" [shape=oval,style=filled,fillcolor=gray82," + fontattr + "] ;\n"
    elif typestr == "interface":
        return "\"" + n + "\" [shape=rectangle,style=filled,fillcolor=lightsteelblue," + fontattr + "] ;\n"

def gen_graphviz(blobs):
    for (b, d) in blobs.items():
        nodes = []
        edges = ""

        nodes.append(gen_node(b, d["type"]))

        (libs, ifdeps, ifexps) = d["deps"]
        for l in libs:
            nodes.append(gen_node(l, "library"))
            edges += "\"" + b + "\" -> \"" + l + "\" ;\n"
        for ifd in ifdeps:
            nodes.append(gen_node(ifd, "interface"))
            edges += "\"" + b + "\" -> \"" + ifd + "\" ;\n"
        for ife in ifexps:
            nodes.append(gen_node(ife + "\n(export)", "interface"))
            edges += "\"" + ife + "\n(export)\" -> \"" + b + "\" [fontname=\"Sans serif\",style=dashed] ;\n"

        (gffd, gfpath) = tempfile.mkstemp(suffix=".gf", prefix="cos_docgen_" + b);
        nodes_str = "".join(unique(nodes)) # rely on unique to simplify this logic
        os.write(gffd, header + nodes_str + edges + footer)
        d["gf"]   = gfpath;
        d["gffd"] = gffd;

        (pdffd, pdfpath) = tempfile.mkstemp(suffix=".pdf", prefix="cos_docgen_" + b.replace(".", "_"));
        subprocess.call(['dot', '-Tpdf', gfpath], stdout=pdffd)
        d["pdf"]   = pdfpath
        d["pdffd"] = pdffd

gen_graphviz(compdeps)
gen_graphviz(libdeps)
gen_graphviz(ifdeps)

# At this point, we have all of the pdfs generated for the component
# dependencies, and need to simply generate the markdown that links to
# them. Note, we do *not* cleanup the temporary files created, and
# instead rely on a "rm /tmp/cos_docgen_*" to clean it all up later.
def gen_md(header, blobs):
    output = "\n" + copy.copy(header) + "\n\n"
    for (b, d) in blobs.items():
        output += readfile(d["path"] + "doc.md")
        output += "\n### Dependencies and Exports\n\n![Exports and dependencies for " + b + ". Teal hexagons are *component* implementations, slate rectangles are *interfaces*, and gray ellipses are *libraries*. Dotted lines denote an *export* relation, and solid lines denote a *dependency*.](" + d["pdf"] + ")\n\n"
    return output

comphead=readfile("./resources/component_doc.md")
ifhead  =readfile("./resources/interface_doc.md")
libhead =readfile("./resources/lib_doc.md")

md = gen_md(comphead, compdeps) + gen_md(ifhead, ifdeps) + gen_md(libhead, libdeps)
(mdfd, mdpath) = tempfile.mkstemp(suffix=".md", prefix="cos_docgen_per-blob");
os.write(mdfd, md)

print(mdpath)
