#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpool.o,a2;!l.o,a8;!stat.o,a25;!te.o,a3;!e.o,a3;!smn.o,a2;!va.o,a1;!buf.o,a2;!tp.o,a4;\
(!po_low.o=ppong_lower.o), ;(!po.o=ppong.o), ;(!pi.o=pingp.o),a9;(!pi2.o=pingp.o),a10:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
cg.o-fprr.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o|smn.o|va.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|schedconf.o|cg.o;\
va.o-print.o|fprr.o|mm.o|boot.o|l.o;\
smn.o-print.o|fprr.o|mm.o|boot.o|va.o|mpool.o|l.o;\
buf.o-fprr.o|print.o|l.o|mm.o|boot.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
tp.o-smn.o|buf.o|print.o|te.o|fprr.o|schedconf.o|mm.o|va.o|mpool.o;\
\
pi.o-smn.o|po.o|print.o|fprr.o|va.o|mm.o|buf.o|l.o;\
pi2.o-smn.o|po.o|print.o|fprr.o|va.o|mm.o|buf.o|l.o;\
po.o-smn.o|va.o|mm.o|print.o|buf.o|fprr.o|l.o|po_low.o;\
po_low.o-smn.o|va.o|mm.o|print.o|buf.o|fprr.o|l.o\
" ./gen_client_stub
