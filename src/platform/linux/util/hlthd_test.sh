#!/bin/sh

# sm.o-fprr.o|print.o|mm.o\

./cos_loader \
"c0.o, ;*fprr.o, ;!l.o,a8;mm.o, ;print.o, ;!te.o,a3;!sm.o,a1;!e.o,a3;schedconf.o, ;!stat.o,a25;st.o, ;bc.o, ;\
boot.o,a4;!hls.o,a4;!hlc.o, :\
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
hlc.o-sm.o|print.o|te.o|fprr.o;\
hls.o-sm.o|print.o|hlc.o|te.o|fprr.o;\
boot.o-print.o|fprr.o|mm.o\
" ./gen_client_stub
