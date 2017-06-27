#!/bin/sh

# translator test

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!mpool.o,a3;!trans.o,a6;!l.o,a1;!te.o,a3;!eg.o,a4;!cbuf.o,a5;!va.o,a2;!utrans.o,a11;!vm.o,a1:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o|va.o;\
eg.o-cbuf.o|fprr.o|print.o|mm.o|l.o|va.o;\
\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
utrans.o-fprr.o|cbuf.o|mm.o|eg.o|va.o|l.o|[print_]trans.o;\
trans.o-fprr.o|l.o|cbuf.o|mm.o|va.o|eg.o|print.o\
" ./gen_client_stub
