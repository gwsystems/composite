#!/bin/sh

# torrent test

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpool.o,a3;!trans.o,a6;!l.o,a1;!te.o,a3;!e.o,a4;!stat.o,a25;!cbuf.o,a5;!rfs.o,a7;!tt.o,a8;!va.o,a2;!vm.o,a1:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|va.o;\
stat.o-cbuf.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
rfs.o-fprr.o|print.o|mm.o|cbuf.o|l.o|e.o|va.o;\
tt.o-fprr.o|rfs.o|cbuf.o|mm.o|e.o|va.o|l.o|print.o;\
trans.o-fprr.o|l.o|cbuf.o|mm.o|va.o|e.o|print.o;\
cg.o-fprr.o\
" ./gen_client_stub

#mpd.o-cbuf.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
#!mpd.o,a5;
#[print_]trans.o
