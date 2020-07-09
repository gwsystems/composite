#!/bin/python

import glob
import re
import string

chapters = sorted(glob.glob("[0-9][0-9]_*.md"))
version  = glob.glob("composite_dev_manual_v*.pdf")[0]

header = "# Composite Developer Manual\n\nPlease see the [pdf](./" + version + ") for a nicely rendered version of this manual.\n\n## Table of Contents\n\n"

toc = ""
for c in chapters:
    underscores = re.split("\.", string.rstrip(c))[0]
    word_list   = re.split("_", underscores)[1:]
    words       = " ".join(word_list)
    toc += "- [" + words.capitalize() + "](./" + c + ")\n"

print(header + toc)
