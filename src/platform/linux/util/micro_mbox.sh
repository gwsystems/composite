#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!mpool.o,a3;!tasc.o,a6;!l.o,a1;!te.o,a3;!eg.o,a4;!cbuf.o,a5;!, a6;!micro_mbox_server.o,a7 '5 3';!micro_mbox_client.o,a9 '10 7';!vm.o,a1;!va.o,a2:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o|va.o;\
eg.o-cbuf.o|fprr.o|print.o|mm.o|l.o|va.o;\
\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
micro_mbox_server.o-fprr.o|tasc.o|cbuf.o||mm.o|eg.o|va.o|l.o|print.o|te.o;\
micro_mbox_client.o-fprr.o|tasc.o|cbuf.o||mm.o|eg.o|va.o|l.o|print.o|te.o;\
tasc.o-fprr.o|l.o|cbuf.o||mm.o|va.o|eg.o|print.o\
" ./gen_client_stub
