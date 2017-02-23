#!/bin/sh

# malloc_comp which is getting forked is hard-coded to be spdid 14. 
# Bear that in mind if changing this

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!cbuf.o,a5;!va.o, a2;!l.o,a1;!te.o,a3;!mpool.o, a3;!vm.o, a1;\
!cbboot.o,a6;!forkpong.o, ;!forkping.o, ;!forkvoter.o,a9:\
c0.o-llboot.o;\
print.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
mpool.o-print.o|fprr.o|mm.o|va.o|l.o|llboot.o;\
vm.o-fprr.o|print.o|mm.o|l.o|llboot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o|llboot.o;\
te.o-print.o|fprr.o|mm.o|va.o;\
cbuf.o-fprr.o|print.o|l.o|mm.o|va.o|llboot.o;\
cbboot.o-print.o|fprr.o|mm.o|boot.o|cbuf.o|[parent_]llboot.o;\
forkpong.o-fprr.o|cbuf.o|va.o|forkvoter.o|print.o|cbboot.o;\
forkping.o-fprr.o|cbuf.o|va.o|forkvoter.o|print.o|cbboot.o;\
forkvoter.o-te.o|fprr.o|cbuf.o|va.o|print.o|cbboot.o\
" ./gen_client_stub
