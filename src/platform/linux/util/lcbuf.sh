#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!sm.o,a2;!va.o,a1;!l.o,a5;!te.o,a3;!e.o,a3;!stat.o,a25;!buf.o,a6;!cbc.o,a7;!cbs.o, :\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-sm.o|fprr.o|mm.o|print.o|te.o|va.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
va.o-print.o|fprr.o|mm.o|boot.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o;\
buf.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o;\
cbc.o-sm.o|fprr.o|mm.o|print.o|buf.o|cbs.o|va.o;\
cbs.o-sm.o|mm.o|print.o|buf.o|va.o;\
cg.o-fprr.o\
" ./gen_client_stub
