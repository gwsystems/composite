#!/bin/sh

# argument: component object file
# output: all undefined symbols (including data)

#nm $1 | awk '{if (NF==2 && $1 == "U") print $2}'
nm -u $1 | awk '{print $2}'
