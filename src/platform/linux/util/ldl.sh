#!/bin/sh

# test periodic_read -- download something!

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o,a2;schedconf.o, ;cg.o,a1;bc.o, ;st.o, ;\
\
!mpd.o,a5;!stat.o,a25;!if.o,a5;!ip.o, ;!port.o, ;!l.o,a4;!te.o,a3;\
!net.o,a6;!e.o,a5;!va.o,a2;!pr.o,a8;!vm.o,a1:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
net.o-cbuf.o|fprr.o|mm.o|print.o|l.o|te.o|e.o|ip.o|port.o|va.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|st.o|va.o;\
stat.o-cbuf.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
ip.o-cbuf.o|if.o;\
port.o-cbuf.o|l.o;\
if.o-cbuf.o|print.o|mm.o|va.o|l.o|fprr.o;\
schedconf.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
\
mpd.o-cbuf.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
cg.o-fprr.o;\
pr.o-cbuf.o|print.o|e.o|net.o|l.o|fprr.o|mm.o|va.o|te.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
bc.o-print.o\
" ./gen_client_stub


