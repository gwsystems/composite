#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpd.o,a5;!l.o,a5;!te.o,a3;!e.o,a3;!stat.o,a25;!vm.o,a6;!vt.o,a7:\
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
boot.o-print.o|fprr.o|mm.o|cg.o;\
vm.o-cbuf.o|fprr.o|print.o|mm.o|l.o;\
\
mpd.o-cbuf.o|cg.o|fprr.o|print.o|te.o|mm.o;\
vt.o-cbuf.o|vm.o|fprr.o|print.o|mm.o;\
cg.o-fprr.o\
" ./gen_client_stub
