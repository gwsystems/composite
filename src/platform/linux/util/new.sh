#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!acap_ppong.o, ;!te.o, a5;!va.o, a2;!l.o,a1;!mpool.o, a3;!sm.o, a4;!vm.o, a1;!parmgr.o, a5;!par_bench.o, a10:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
acap_ppong.o-print.o|fprr.o|mm.o|parmgr.o|va.o|sm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
\
par_bench.o-sm.o|va.o|print.o|parmgr.o|fprr.o|mm.o|te.o|l.o|acap_ppong.o;\
parmgr.o-sm.o|va.o|print.o|fprr.o|mm.o\
" ./gen_client_stub



