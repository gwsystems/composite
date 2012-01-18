#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!smn.o,a2;!sp.o,a4;!va.o,a1;!l.o,a8;!te.o,a3;!e.o,a3;!stat.o,a25;!cbp.o,a4;!buf.o,a2;!cbc.o,a10;!cbs1.o, ;!cbs.o, :\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-smn.o|fprr.o|mm.o|print.o|te.o|va.o;\
te.o-smn.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-smn.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-smn.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
va.o-print.o|fprr.o|mm.o|boot.o;\
smn.o-print.o|fprr.o|mm.o|boot.o|va.o;\
buf.o-smn.o|fprr.o|print.o|l.o|mm.o|va.o;\
cbc.o-smn.o|fprr.o|mm.o|print.o|buf.o|cbs.o|cbs1.o|va.o;\
cbs.o-smn.o|mm.o|print.o|buf.o|va.o;\
cbs1.o-smn.o|mm.o|print.o|buf.o|va.o;\
cbp.o-smn.o|buf.o|print.o|te.o|fprr.o|schedconf.o|mm.o|va.o;\
sp.o-smn.o|print.o|te.o|fprr.o|schedconf.o|mm.o|va.o;\
cg.o-fprr.o\
" ./gen_client_stub
