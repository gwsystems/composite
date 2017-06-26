#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;\
\
!l.o,a8;!te.o,a3;!e.o,a3;!stat.o,a25;(!po.o=ppong.o), ;(!pi.o=pingp.o),a4:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-cbuf.o|fprr.o|mm.o|print.o|te.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-cbuf.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
pi.o-cbuf.o|po.o|print.o|fprr.o;\
po.o-cbuf.o;\
boot.o-print.o|fprr.o|mm.o;\
cbuf.o-print.o|fprr.o|mm.o|boot.o\
" ./gen_client_stub
