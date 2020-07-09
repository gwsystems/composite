#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!l.o,a1;!mpool.o,a3;!cbuf.o,a5;!va.o,a2;!vm.o,a1;!malloc_cbuf.o, :\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
l.o-fprr.o|mm.o|print.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
malloc_cbuf.o-fprr.o|l.o|cbuf.o||mm.o|va.o|print.o\
" ./gen_client_stub
