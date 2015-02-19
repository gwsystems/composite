#!/bin/shp

# ping pong using membrane

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!mpool.o,a3;!l.o,a1;(!po.o=ppong.o), ;!trans_ctol_meas.o,a9;!va.o,a2;!vm.o,a1:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o;\
\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
\
trans_ctol_meas.o-cbuf.o|fprr.o|va.o|print.o|l.o|po.o;\
po.o-cbuf.o|va.o|print.o\
" ./gen_client_stub

#rfs.o-fprr.o|print.o|mm.o|cbuf.o|l.o|e.o|va.o;\
#tt.o-fprr.o|rfs.o|cbuf.o|mm.o|e.o|va.o|l.o|print.o;\
