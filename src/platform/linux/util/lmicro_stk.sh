#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!mpool.o,a3;!trans.o,a6;!sm.o,a4;!l.o,a1;!te.o,a3;!e.o,a4;!tp.o,a6;!stat.o,a25;!buf.o,a5;(!bot.o=micbuf3.o), ;(!mid.o=micbuf2.o), ;(!top.o=micro_stk.o),a9;!va.o,a2;!vm.o,a1;!bufp.o, :\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
boot.o-print.o|fprr.o|mm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
bufp.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|buf.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
tp.o-sm.o|buf.o|print.o|te.o|fprr.o|mm.o|va.o|mpool.o;\
trans.o-sm.o|fprr.o|l.o|buf.o|bufp.o|mm.o|va.o|e.o|print.o;\
\
top.o-sm.o|fprr.o|mid.o|print.o;\
mid.o-sm.o|fprr.o|print.o|bot.o;\
bot.o-sm.o|fprr.o|print.o|buf.o|va.o|l.o|mm.o|bufp.o\
" ./gen_client_stub
