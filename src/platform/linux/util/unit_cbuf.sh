#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!l.o,a1;!va.o,a2;!mpool.o,a3;!te.o,a3;!sm.o,a4;!e.o,a4;!cbuf.o,a5;!ucbuf1.o,a10;!ucbuf2.o, ;!ucbufp.o,a9;!stat.o,a25;!vm.o,a1:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
cbuf.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
ucbuf1.o-fprr.o|sm.o|ucbuf2.o|ucbufp.o|print.o|mm.o|va.o|cbuf.o|l.o;\
ucbuf2.o-sm.o|print.o|mm.o|va.o|cbuf.o|l.o;\
ucbufp.o-fprr.o|sm.o|print.o|mm.o|va.o|cbuf.o|l.o\
" ./gen_client_stub

#mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
#!mpd.o,a5;
#[print_]trans.o
