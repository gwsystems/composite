#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!mpd.o, a5;!cpu.o, ;(!po.o=ppong.o), ;(!pi.o=pingp.o), a9;!va.o, a2;!l.o,a1;!mpool.o, a3;!sm.o, a4;!e.o,a4:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
cpu.o-print.o|fprr.o|mm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
l.o-fprr.o|mm.o|print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o;\
mpd.o-sm.o|fprr.o|print.o|mm.o|va.o;\
\
pi.o-sm.o|fprr.o|va.o|po.o|print.o;\
po.o-sm.o|fprr.o|va.o|print.o|e.o\
" ./gen_client_stub
