#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!cpu.o, ;(!cpu1.o=cpu.o), ;!te.o, a5;(!po.o=acap_ppong.o), ;(!pi.o=acap_pingp.o), a9;!va.o, a2;!l.o,a1;!mpool.o, a3;!sm.o, a4;!acap.o, a5;!cos_lu.o, a11;!fft.o, a9;!cos_jacobi.o, a12;!omp_comp.o, a10:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
cpu.o-print.o|fprr.o|mm.o;\
cpu1.o-print.o|fprr.o|mm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
\
pi.o-sm.o|fprr.o|va.o|po.o|print.o|acap.o|mm.o;\
po.o-sm.o|va.o|print.o|acap.o|mm.o|fprr.o;\
omp_comp.o-sm.o|va.o|print.o|acap.o|fprr.o|mm.o|te.o|l.o;\
cos_lu.o-sm.o|va.o|print.o|acap.o|fprr.o|mm.o|te.o|l.o;\
fft.o-sm.o|va.o|print.o|acap.o|fprr.o|mm.o|te.o|l.o;\
cos_jacobi.o-sm.o|va.o|print.o|acap.o|fprr.o|mm.o|te.o|l.o;\
acap.o-sm.o|va.o|print.o|fprr.o|mm.o\
" ./gen_client_stub



