#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!pfr.o, ;!upf.o, ;!pft.o,a10:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
pfr.o-print.o|fprr.o|mm.o|boot.o;\
upf.o-print.o|fprr.o|pfr.o;\
pft.o-print.o|fprr.o|pfr.o|upf.o\
" ./gen_client_stub

