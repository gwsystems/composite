#!/bin/shp

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!mpool.o,a3;!sm.o,a4;!l.o,a1;!if.o,a9;!va.o,a2:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
\
if.o-sm.o|fprr.o|mm.o|va.o|print.o|l.o\
" ./gen_client_stub

#rfs.o-sm.o|fprr.o|print.o|mm.o|buf.o|l.o|e.o|va.o;\
#tt.o-sm.o|fprr.o|rfs.o|buf.o|mm.o|e.o|va.o|l.o|print.o;\
