#!/bin/python

# Generate the per-interface, library, and component documentation.

import glob
import subprocess
import re
import tempfile
import os
import copy

dep_script = "../../src/components/cidl/calculate_dependencies.py"
comp_base = "../../src/components/"
components = comp_base + "implementation/*/*/doc.md"
libraries = comp_base + "lib/*/doc.md"
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
        s = re.split("/", p.rstrip())
        pathified = "/".join(s[0:-1]) + "/"
        removed.append(pathified)
    return removed


def filter_out_skel(p):
    s = re.split("/", p.rstrip())
    s = list(filter(lambda e: e != "", s))
    return not (s[-1] == "skel" or s[-2] == "skel") and not (s[-2] == "archives")


cs = filter(filter_out_skel, remove_trailing_doc(components))
ls = filter(filter_out_skel, remove_trailing_doc(libraries))
ifs = filter(filter_out_skel, remove_trailing_doc(interfaces))


def unique(l):
    return list(set(l))


# listify the output
def clean_dep_output(lst):
    if isinstance(lst, bytes):
        lst = lst.decode("utf-8")
    return filter(lambda d: d != "", unique(re.split(" ", lst.rstrip())))


def gather_deps(path):
    libdeps = clean_dep_output(
        subprocess.check_output(
            ["python", dep_script, path, comp_base, "shallowlibdeps"]
        )
    )
    ifdeps = clean_dep_output(
        subprocess.check_output(
            ["python", dep_script, path, comp_base, "shallowifdeps"]
        )
    )
    ifexps = clean_dep_output(
        subprocess.check_output(
            ["python", dep_script, path, comp_base, "shallowifexps"]
        )
    )
    return (libdeps, ifdeps, ifexps)


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
    compdeps[comp_name(c)] = {"deps": gather_deps(c), "type": "component", "path": c}
for l in ls:
    libdeps[lib_or_if_name(l)] = {"deps": gather_deps(l), "type": "library", "path": l}
for i in ifs:
    ifdeps[lib_or_if_name(i)] = {"deps": gather_deps(i), "type": "interface", "path": i}

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
    fontattr = 'fontname="Sans serif"'
    if typestr == "component":
        return (
            '"'
            + n
            + '" [shape=hexagon,style=filled,fillcolor=lightblue,'
            + fontattr
            + "] ;\n"
        )
    elif typestr == "library":
        return (
            '"'
            + n
            + '" [shape=oval,style=filled,fillcolor=gray82,'
            + fontattr
            + "] ;\n"
        )
    elif typestr == "interface":
        return (
            '"'
            + n
            + '" [shape=rectangle,style=filled,fillcolor=lightsteelblue,'
            + fontattr
            + "] ;\n"
        )


def gen_graphviz(blobs):
    for (b, d) in blobs.items():
        nodes = []
        edges = ""

        nodes.append(gen_node(b, d["type"]))

        (libs, ifdeps, ifexps) = d["deps"]
        for l in libs:
            nodes.append(gen_node(l, "library"))
            edges += '"' + b + '" -> "' + l + '" ;\n'
        for ifd in ifdeps:
            nodes.append(gen_node(ifd, "interface"))
            edges += '"' + b + '" -> "' + ifd + '" ;\n'
        for ife in ifexps:
            nodes.append(gen_node(ife + "\n(export)", "interface"))
            edges += (
                '"'
                + ife
                + '\n(export)" -> "'
                + b
                + '" [fontname="Sans serif",style=dashed] ;\n'
            )

        (gffd, gfpath) = tempfile.mkstemp(suffix=".gf", prefix="cos_docgen_" + b)
        nodes_str = "".join(unique(nodes))  # rely on unique to simplify this logic
        content = header + nodes_str + edges + footer
        if isinstance(content, str):
            content = content.encode("utf-8")
        os.write(gffd, content)
        d["gf"] = gfpath
        d["gffd"] = gffd

        (pdffd, pdfpath) = tempfile.mkstemp(
            suffix=".pdf", prefix="cos_docgen_" + b.replace(".", "_")
        )
        subprocess.call(["dot", "-Tpdf", gfpath], stdout=pdffd)
        d["pdf"] = pdfpath
        d["pdffd"] = pdffd


gen_graphviz(compdeps)
gen_graphviz(libdeps)
gen_graphviz(ifdeps)

# Assumes that comments have the *exact* form (space sensitive):
# /** <- comment starts
#  * ...comment...
#  */ <- comment ends
# void function_prototypes(void); <- same line
# static inline void
# inline_function_prototype(void) <- no ; or {
# { <- alone on line
#     ...
# } <- alone on line
# /***/ <- end prototypes
#
# This simply translates that into:
# ``` c
# void function_prototypes(void);
# static inline void
# inline_function_prototype(void)
# ```
# ...comment...
#
# This assumes and leverages the style perscribed by the CSG.

library_headers = comp_base + "lib/*/*.h"
interface_headers = comp_base + "interface/*/*.h"
### Auto-gen the documentation for the interfaces and libraries
def gen_doc(header):
    doc_header = "### API Documentation\n\n"
    doc = ""
    proto_head = "\n``` c\n"
    proto_tail = "```\n\n"
    header_comment = ""
    comment = ""
    proto = ""
    state = "scanning"

    def de_comment(line):
        l = re.sub(r"^ \*", r"", line)  # get rid of the leading " *"
        if len(l) > 0 and l[0] == " ":
            return l[1:]
        return l

    def commit_proto(proto, comment):
        if proto != "":
            proto = proto_head + proto + proto_tail
        ret = proto + comment
        if ret != "":
            ret = "\n---\n" + ret
        return ret

    in_comment = False
    for l in re.split("\n", header):
        # First, filter out # directives and comments
        if re.match("^#.*", l) != None:
            continue  # skip #endif/#define etc...
        c_single = re.match(r"(.*)//.*", l)  # remove text following //
        if c_single != None:
            l = c_single.group(0)
        c_start = re.match(r"(.*)/\*[^\*].*", l)  # remove all after /*
        c_end = re.match(r".*\*/(.*)", l)  # leave only code after */
        if not in_comment and c_start != None:
            in_comment = True
            l = c_start.group(0)
        if in_comment:
            if c_end == None:
                continue
            in_comment = False
            l = c_end.group(0)

        # Next, work through the state machine
        if state == "scanning" and l == "/**":
            doc += commit_proto(proto, comment)
            proto = ""
            comment = ""
            state = "comment"
        elif state == "scanning" and l == "/***":
            doc += commit_proto(proto, comment)
            proto = ""
            comment = ""
            state = "header comment"
        elif state == "comment":
            if l == " */":
                state = "prototypes"
            else:
                l = de_comment(l)
                comment += l + "\n"
        elif state == "header comment":
            if l == " */":
                state = "scanning"
            else:
                l = de_comment(l)
                header_comment += l + "\n"
        elif state == "prototypes":
            p = re.search(r"^(.*);$", l)
            if l == "/**":
                doc += commit_proto(proto, comment)
                proto = ""
                comment = ""
                state = "comment"
            elif p != None:
                proto += p.group(0) + "\n"
            elif l == "{":
                proto += "{ ... "
                state = "function body"
            elif l == "/***/":
                doc += commit_proto(proto, comment)
                proto = ""
                comment = ""
                state = "scanning"
            else:
                proto += l + "\n"
        elif state == "function body":
            if l == "}":
                proto += "}\n"
                state = "prototypes"

    doc += commit_proto(proto, comment)

    return doc_header + header_comment + doc


for (b, d) in ifdeps.items():
    path = d["path"] + b + ".h"
    contents = readfile(path)
    doc = gen_doc(contents)
    d["doc"] = doc

for (b, d) in libdeps.items():
    path = d["path"] + b + ".h"
    contents = readfile(path)
    doc = gen_doc(contents)
    d["doc"] = doc

# At this point, we have all of the pdfs generated for the component
# dependencies, and need to simply generate the markdown that links to
# them. Note, we do *not* cleanup the temporary files created, and
# instead rely on a "rm /tmp/cos_docgen_*" to clean it all up later.
def gen_md(header, blobs):
    output = "\n" + copy.copy(header) + "\n\n"
    for (b, d) in blobs.items():
        output += readfile(d["path"] + "doc.md")
        output += (
            "\n### Dependencies and Exports\n\n![Exports and dependencies for "
            + b
            + ". Teal hexagons are *component* implementations, slate rectangles are *interfaces*, and gray ellipses are *libraries*. Dotted lines denote an *export* relation, and solid lines denote a *dependency*.]("
            + d["pdf"]
            + ")\n\n"
        )
        if "doc" in d:
            output += d["doc"] + "\n\n"
    return output


comphead = readfile("./resources/component_doc.md")
ifhead = readfile("./resources/interface_doc.md")
libhead = readfile("./resources/lib_doc.md")

md = gen_md(comphead, compdeps) + gen_md(ifhead, ifdeps) + gen_md(libhead, libdeps)
if isinstance(md, str):
    md = md.encode("utf-8")
(mdfd, mdpath) = tempfile.mkstemp(suffix=".md", prefix="cos_docgen_per-blob")
os.write(mdfd, md)

print(mdpath)
