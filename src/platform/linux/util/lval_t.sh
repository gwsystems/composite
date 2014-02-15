#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;vm.o,a5;\
\
!mpd.o,a5;!sm.o,a1;!l.o,a5;!te.o,a3;!e.o,a3;!stat.o,a25;!va.o,a6;!vat.o,a7:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-sm.o|fprr.o|mm.o|print.o|te.o;\
te.o-sm.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
vm.o-sm.o|fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-sm.o|fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o;\
mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o;\
vat.o-sm.o|va.o|fprr.o|print.o|mm.o;\
cg.o-fprr.o\
" ./gen_client_stub
