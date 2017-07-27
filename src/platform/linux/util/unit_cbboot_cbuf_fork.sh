#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!pfr.o,a2;!l.o,a1;!mpool.o,a3;!te.o,a3;!e.o,a4;!cbuf.o,a5;!va.o,a2;!vm.o,a1;!cbboot.o,a6;!ucbuf1.o,a10;!ucbuf2.o, ;!ucbufptf.o,a9;!stat.o,a25:\
\
c0.o-llboot.o;\
print.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o|llboot.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o|va.o|llboot.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|va.o|llboot.o;\
stat.o-cbuf.o|te.o|fprr.o|l.o|print.o|e.o|cbboot.o;\
pfr.o-fprr.o|mm.o|print.o|boot.o|[parent_]llboot.o;\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|llboot.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|llboot.o;\
cbboot.o-print.o|fprr.o|mm.o|boot.o|cbuf.o|[parent_]llboot.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o|llboot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o|llboot.o;\
ucbuf1.o-fprr.o|ucbuf2.o|ucbufptf.o|print.o|mm.o|va.o|cbuf.o|cbboot.o;\
ucbuf2.o-fprr.o|print.o|mm.o|va.o|cbuf.o|cbboot.o;\
ucbufptf.o-fprr.o|print.o|mm.o|va.o|cbuf.o|te.o|cbboot.o\
" ./gen_client_stub

#mpd.o-cbuf.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
#!mpd.o,a5;
#[print_]trans.o
