#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;!sm.o,a1;!l.o,a8;mm.o, ;print.o, ;!te.o,a3;!e.o,a3;schedconf.o, ;!stat.o,a25;st.o, ;bc.o, ;\
boot.o,a4;(!po.o=ppong.o), ;(!pi.o=pingp.o),a4:\
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
pi.o-sm.o|po.o|print.o|fprr.o;\
po.o-sm.o;\
boot.o-print.o|fprr.o|mm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o\
" ./gen_client_stub
