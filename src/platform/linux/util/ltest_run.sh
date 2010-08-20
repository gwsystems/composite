#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;!l.o,a6;mm.o, ;print.o, ;!te.o,a4;!test.o,a8;!e.o,a7;schedconf.o, ;!stat.o,a25;st.o, ;bc.o, ;!sm.o,a3;!sp.o,a5;boot.o,a2:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-fprr.o|mm.o|print.o|te.o|sm.o;\
te.o-print.o|fprr.o|mm.o|sm.o;\
mm.o-print.o;\
test.o-print.o|fprr.o|sm.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o|sm.o;\
stat.o-te.o|fprr.o|l.o|print.o|e.o|sm.o;\
st.o-print.o;\
schedconf.o-print.o;\
sm.o-print.o|mm.o|fprr.o|boot.o;\
sp.o-te.o|fprr.o|schedconf.o|print.o|mm.o|sm.o;\
boot.o-print.o|fprr.o|mm.o|schedconf.o;\
bc.o-print.o\
" ./gen_client_stub
