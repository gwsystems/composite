#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!cpu.o, ;!te.o, a5;!va.o, a2;!l.o,a1;!mpool.o, a3;!sm.o, a4;!vm.o, a1;!parmgr.o, a5;!cos_lu.o, a11;!fft.o, a9;!cos_jacobi.o, a12;!omp_comp.o, a10:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
cpu.o-print.o|fprr.o|mm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
\
omp_comp.o-sm.o|va.o|print.o|parmgr.o|fprr.o|mm.o|te.o|l.o;\
cos_lu.o-sm.o|va.o|print.o|parmgr.o|fprr.o|mm.o|te.o|l.o;\
fft.o-sm.o|va.o|print.o|parmgr.o|fprr.o|mm.o|te.o|l.o;\
cos_jacobi.o-sm.o|va.o|print.o|parmgr.o|fprr.o|mm.o|te.o|l.o;\
parmgr.o-sm.o|va.o|print.o|fprr.o|mm.o\
" ./gen_client_stub



