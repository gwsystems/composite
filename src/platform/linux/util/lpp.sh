#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
(!po.o=ppong.o), ;(!pi.o=pingp.o), a9;!cbuf.o,a5;!va.o, a2;!l.o,a1;!mpool.o, a3;!vm.o, a1:\
c0.o-llboot.o;\
print.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
l.o-fprr.o|mm.o|print.o;\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|llboot.o;\
pi.o-cbuf.o|fprr.o|va.o|po.o|print.o;\
po.o-cbuf.o|va.o|print.o\
" ./gen_client_stub
