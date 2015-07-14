#!/bin/sh

./cos_linker \
"c0.o, ;llboot.o, ;llping.o, ;mm_parsec.o, :\
llping.o-mm_parsec.o;\
c0.o-llboot.o\
" ./gen_client_stub






