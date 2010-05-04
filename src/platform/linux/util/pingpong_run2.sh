#!/bin/sh

# sm.o-fprr.o|print.o|mm.o\

./cos_loader \
"c0.o, ;*fprr.o, ;sm.o,a1;l.o,a8;mm.o, ;print.o, ;te.o,a3;e.o,a3;schedconf.o, ;stat.o,a25;st.o, ;bc.o, ;\
pingp.o,a4;ppong.o, :\
\
c0.o-sm.o|fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-sm.o|fprr.o|mm.o|print.o|te.o;\
te.o-sm.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
pingp.o-sm.o|ppong.o|print.o|fprr.o;\
ppong.o-sm.o;\
sm.o-print.o|mm.o|fprr.o\
" ./gen_client_stub
