#!/bin/sh

# torrent test

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;initfs.o,a3;\
\
!mpool.o,a3;!trans.o,a6;!sm.o,a4;!l.o,a1;!te.o,a3;!e.o,a4;!stat.o,a25;!buf.o,a5;!tp.o,a6;!rotar.o,a7;!tart.o,a8;!va.o,a2:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
initfs.o-fprr.o|print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
tp.o-sm.o|buf.o|print.o|te.o|fprr.o|schedconf.o|mm.o|va.o|mpool.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
rotar.o-sm.o|fprr.o|print.o|mm.o|buf.o|l.o|e.o|va.o|initfs.o;\
tart.o-sm.o|fprr.o|buf.o|mm.o|e.o|va.o|l.o|print.o|rotar.o;\
trans.o-sm.o|fprr.o|l.o|buf.o|mm.o|va.o|e.o|print.o;\
cg.o-fprr.o\
" ./gen_client_stub

#tt.o-sm.o|fprr.o|rfs.o|buf.o|mm.o|e.o|va.o|l.o|print.o;\
#mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
#!mpd.o,a5;
#[print_]trans.o
