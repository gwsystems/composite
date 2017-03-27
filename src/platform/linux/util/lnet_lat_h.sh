#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpd.o,a5;!sm.o,a1;!l.o,a5;!te.o,a3;!e.o,a3;!stat.o,a25;!if.o,a5;!nr.o,a6;\
\
(*fprrc2.o=fprr.o),a6;(*fprrc3.o=fprr.o),a4;(*fprrc4.o=fprr.o),a4:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
fprrc2.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
fprrc3.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc2.o;\
fprrc4.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc3.o;\
l.o-sm.o|fprr.o|mm.o|print.o|te.o;\
te.o-sm.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
if.o-sm.o|print.o|mm.o|l.o|fprrc4.o;\
nr.o-sm.o|print.o|fprrc4.o|if.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
sm.o-print.o|fprr.o|mm.o|boot.o;\
mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o;\
cg.o-fprr.o\
" ./gen_client_stub
