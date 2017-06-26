#!/bin/shp

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!mpool.o,a3;!l.o,a1;!if.o,a9;!va.o,a2;!vm.o,a1:\
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
tif.o-print.o|fprr.o|mm.o|l.o|va.o|eg.o|cbuf.o;\ 
" ./gen_client_stub

#rfs.o-fprr.o|print.o|mm.o|cbuf.o|l.o|e.o|va.o;\
#tt.o-fprr.o|rfs.o|cbuf.o|mm.o|e.o|va.o|l.o|print.o;\
