#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!pfr.o, ;!upf.o, ;!l.o,a1;!vm.o,a1;!va.o,a2;!cbuf.o,a5;!cbboot.o,a6;!pftc.o,a10:\
c0.o-llboot.o;\
print.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
pfr.o-print.o|fprr.o|boot.o;\
upf.o-print.o|fprr.o|pfr.o;\
l.o-fprr.o|mm.o|print.o;\
vm.o-fprr.o|print.o|mm.o|l.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
cbuf.o-fprr.o|print.o|l.o|mm.o|va.o;\
cbboot.o-print.o|fprr.o|mm.o|boot.o|cbuf.o;\
pftc.o-print.o|fprr.o|pfr.o|upf.o|cbboot.o|cbuf.o\
" ./gen_client_stub

