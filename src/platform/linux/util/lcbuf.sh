#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpd.o,a5;!sm.o,a1;!l.o,a5;!te.o,a3;!e.o,a3;!stat.o,a25;!buf.o,a6;!cbc.o,a7:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-sm.o|fprr.o|mm.o|print.o|te.o;\
te.o-sm.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
sm.o-print.o|fprr.o|mm.o|boot.o;\
mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o;\
buf.o-sm.o|fprr.o|print.o|l.o|mm.o;\
cbc.o-sm.o|fprr.o|mm.o|print.o|buf.o;\
cg.o-fprr.o\
" ./gen_client_stub
