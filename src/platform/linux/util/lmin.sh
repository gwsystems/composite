#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;!cpu.o, ;(!cpu1.o=cpu.o), :\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o;\
cpu.o-print.o|fprr.o|mm.o;\
cpu1.o-print.o|fprr.o|mm.o\
" ./gen_client_stub


