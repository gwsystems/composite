#!/bin/sh

# torrent test

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpd.o,a5;!sm.o,a3;!l.o,a1;!te.o,a3;!e.o,a4;!stat.o,a25;!buf.o,a5;!rfs.o,a6;!tt.o,a7;!va.o,a2:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
buf.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
rfs.o-sm.o|fprr.o|print.o|mm.o|buf.o|l.o|e.o|va.o;\
tt.o-sm.o|fprr.o|print.o|buf.o|mm.o|e.o|rfs.o|va.o;\
cg.o-fprr.o\
" ./gen_client_stub
