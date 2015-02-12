#!/bin/sh

# torrent test

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!initfs.o,a3;!mpool.o,a3;!trans.o,a6;!sm.o,a4;!l.o,a1;!te.o,a3;!eg.o,a4;!cbuf.o,a5;!rotar.o,a7;!tart.o,a8;!va.o,a2;!vm.o,a1:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o|[parent_]llboot.o;\
eg.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o;\
initfs.o-fprr.o|print.o|cbuf.o||va.o|l.o|mm.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
cbuf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
rotar.o-sm.o|fprr.o|print.o|mm.o|cbuf.o||l.o|eg.o|va.o|initfs.o;\
tart.o-sm.o|fprr.o|cbuf.o||mm.o|eg.o|va.o|l.o|print.o|rotar.o;\
trans.o-sm.o|fprr.o|l.o|cbuf.o||mm.o|va.o|eg.o|print.o\
" ./gen_client_stub

#tt.o-sm.o|fprr.o|rfs.o|cbuf.o|mm.o|e.o|va.o|l.o|print.o;\
#mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
#!mpd.o,a5;
#[print_]trans.o
