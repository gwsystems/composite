#!/bin/sh

# ping pong using membrane

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!mpool.o,a3;!sm.o,a4;!l.o,a1;(!po.o=ppong.o), ;(!pi1.o=ping_mb.o),a9;(!pi2.o=ping_mb.o),a9;!mb.o,a9;!va.o,a2;!vm.o,a1:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
\
mb.o-sm.o|fprr.o|va.o|po.o|print.o|l.o;\
pi1.o-sm.o|fprr.o|va.o|mb.o|print.o;\
pi2.o-sm.o|fprr.o|va.o|mb.o|print.o;\
po.o-sm.o|va.o|print.o\
" ./gen_client_stub

#rfs.o-sm.o|fprr.o|print.o|mm.o|cbuf.o|l.o|e.o|va.o;\
#tt.o-sm.o|fprr.o|rfs.o|cbuf.o|mm.o|e.o|va.o|l.o|print.o;\
