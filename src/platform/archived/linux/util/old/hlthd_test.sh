#!/bin/sh

# cbuf.o-fprr.o|print.o|mm.o\

./cos_loader \
"c0.o, ;*fprr.o, ;!l.o,a8;mm.o, ;print.o, ;!te.o,a3;!e.o,a3;schedconf.o, ;!stat.o,a25;st.o, ;bc.o, ;\
boot.o,a4;!hls.o,a7;!hlc.o, ;!sp.o,a5:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-cbuf.o|fprr.o|mm.o|print.o|te.o;\
\
te.o-cbuf.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-cbuf.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
hlc.o-cbuf.o|print.o|te.o|fprr.o;\
hls.o-cbuf.o|print.o|hlc.o|te.o|fprr.o;\
sp.o-cbuf.o|print.o|te.o|fprr.o|schedconf.o|mm.o;\
boot.o-print.o|fprr.o|mm.o\
" ./gen_client_stub
