#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!cbuf.o,a5;!va.o,a2;!vm.o,a1;!cbboot.o,a6;!fork_test.o,a10:\
\
c0.o-llboot.o;\
print.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o|llboot.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o|llboot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o|llboot.o;\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|llboot.o;\
cbboot.o-print.o|fprr.o|mm.o|boot.o|cbuf.o|[parent_]llboot.o;\
fork_test.o-fprr.o|print.o|cbboot.o\
" ./gen_client_stub

