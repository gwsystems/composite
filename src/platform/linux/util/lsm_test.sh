#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;cg.o,a1;boot.o,a4;\
!mpool.o,a3;!hls.o,a7;!hlc.o, ;!l.o,a1;!te.o,a3;!e.o,a3;!stat.o,a25;!va.o,a2;!vm.o,a1:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-fprr.o|mm.o|print.o;\
\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|st.o|va.o;\
stat.o-cbuf.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
cg.o-fprr.o;\
hlc.o-cbuf.o|print.o|te.o|fprr.o;\
hls.o-cbuf.o|print.o|hlc.o|te.o|fprr.o;\
boot.o-print.o|fprr.o|mm.o|cg.o\
" ./gen_client_stub
