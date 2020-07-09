#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;!l.o,a6;mm.o, ;print.o, ;!te.o,a4;!test.o,a8;!e.o,a7;schedconf.o, ;!stat.o,a25;st.o, ;bc.o, ;!sp.o,a5;boot.o,a2:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-fprr.o|mm.o|print.o|te.o|cbuf.o;\
te.o-print.o|fprr.o|mm.o|cbuf.o;\
mm.o-print.o;\
test.o-print.o|fprr.o|cbuf.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o|cbuf.o;\
stat.o-te.o|fprr.o|l.o|print.o|e.o|cbuf.o;\
st.o-print.o;\
schedconf.o-print.o;\
\
sp.o-te.o|fprr.o|schedconf.o|print.o|mm.o|cbuf.o;\
boot.o-print.o|fprr.o|mm.o|schedconf.o;\
bc.o-print.o\
" ./gen_client_stub
