#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!cpu.o, ;(!cpu1.o=cpu.o), ;(!po.o=ppong.o), ;(!pi.o=pingp.o), a9;!va.o, a2;!l.o,a1;!mpool.o, a3;!sm.o, a4;!vm.o, a1:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
cpu.o-print.o|fprr.o|mm.o;\
cpu1.o-print.o|fprr.o|mm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
l.o-fprr.o|mm.o|print.o;\
\
pi.o-sm.o|fprr.o|va.o|po.o|print.o;\
po.o-sm.o|va.o|print.o\
" ./gen_client_stub
