#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;cg.o,a1;boot.o,a4;\
!hls.o,a7;!hlc.o, ;!sp.o,a5;!l.o,a8;!te.o,a3;!sm.o,a1;!e.o,a3;!stat.o,a25:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-sm.o|fprr.o|mm.o|print.o|te.o;\
sm.o-print.o|mm.o|fprr.o|boot.o;\
te.o-sm.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
cg.o-fprr.o;\
hlc.o-sm.o|print.o|te.o|fprr.o;\
hls.o-sm.o|print.o|hlc.o|te.o|fprr.o;\
sp.o-sm.o|print.o|te.o|fprr.o|schedconf.o|mm.o;\
boot.o-print.o|fprr.o|mm.o|cg.o\
" ./gen_client_stub
