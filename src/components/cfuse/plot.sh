#!/bin/sh

# the dot file output by haskell has some stupid formatting
sed  -e "s/\\\t/LFCR/g" -e "s/\"//" -e "s/$\"//" -e "s/\\\n//g" -e "s/\\\//g" -e "s/LFCR/\\\n/g" | dot -Tpdf 
