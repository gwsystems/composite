#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpool.o,a2;!smn.o,a2;!va.o,a1;!l.o,a8;!te.o,a3;!e.o,a3;!stat.o,a25;!cbuf.o,a2;!vm.o,a1\
\
(!top0.o=cbf_top.o),a10;\
(!top1.o=cbf_top.o),a10;\
(!top2.o=cbf_top.o),a10;\
(!top3.o=cbf_top.o),a10;\
(!top4.o=cbf_top.o),a10;\
(!top5.o=cbf_top.o),a10;\
\
(!mid0.o=cbf_mid.o), ;\
(!mid1.o=cbf_mid.o), ;\
\
(!bot0.o=cbf_bot.o), :\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-fprr.o|mm.o|print.o|te.o|va.o;\
te.o-smn.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-smn.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-smn.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
vm.o-print.o|fprr.o|mm.o|boot.o;\
va.o-print.o|fprr.o|mm.o|boot.o|vm.o;\
smn.o-print.o|fprr.o|mm.o|boot.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o;\
cbuf.o-fprr.o|print.o|l.o|mm.o|boot.o|va.o|mpool.o;\
\
top0.o-smn.o|fprr.o|mm.o|print.o|cbuf.o|mid0.o|schedconf.o|te.o|va.o;\
top1.o-smn.o|fprr.o|mm.o|print.o|cbuf.o|mid0.o|schedconf.o|te.o|va.o;\
top2.o-smn.o|fprr.o|mm.o|print.o|cbuf.o|mid0.o|schedconf.o|te.o|va.o;\
top3.o-smn.o|fprr.o|mm.o|print.o|cbuf.o|mid1.o|schedconf.o|te.o|va.o;\
top4.o-smn.o|fprr.o|mm.o|print.o|cbuf.o|mid1.o|schedconf.o|te.o|va.o;\
top5.o-smn.o|fprr.o|mm.o|print.o|cbuf.o|mid1.o|schedconf.o|te.o|va.o;\
mid0.o-smn.o|mm.o|print.o|cbuf.o|va.o|bot0.o;\
mid1.o-smn.o|mm.o|print.o|cbuf.o|va.o|bot0.o;\
bot0.o-smn.o|mm.o|print.o|cbuf.o|va.o;\
\
\
cg.o-fprr.o\
" ./gen_client_stub
