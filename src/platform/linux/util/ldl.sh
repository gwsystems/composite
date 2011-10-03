#!/bin/sh

# test periodic_read -- download something!

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o,a2;schedconf.o, ;cg.o,a1;bc.o, ;st.o, ;\
\
!sm.o,a1;!mpd.o,a5;!stat.o,a25;!if.o,a5;!ip.o, ;!port.o, ;!l.o,a4;!te.o,a3;\
!net.o,a6;!e.o,a5;!va.o,a2;!pr.o,a8:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
net.o-sm.o|fprr.o|mm.o|print.o|l.o|te.o|e.o|ip.o|port.o|va.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|st.o|va.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
ip.o-sm.o|if.o;\
port.o-sm.o|l.o;\
if.o-sm.o|print.o|mm.o|va.o|l.o|fprr.o;\
schedconf.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
cg.o-fprr.o;\
pr.o-sm.o|print.o|e.o|net.o|l.o|fprr.o|mm.o|va.o|te.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
bc.o-print.o\
" ./gen_client_stub


