#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mpd.o,a4;!l.o,a8;mm.o, ;print.o, ;te.o,a3;!e.o,a3;schedconf.o, ;!stat.o,a25;st.o, ;bc.o, ;\
boot.o,a4;\
(!sh0.o=sh.o),a10's10000 n10';(!sh1.o=sh.o),'s10000 n10';(!sh2.o=sh.o),'s10000 n10';(!sh3.o=sh.o),'s10000 n10';\
(!sbc0.o=sbc.o), ;(!sbc1.o=sbc.o), ;(!sbc2.o=sbc.o), ;(!sbc3.o=sbc.o), :\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
mpd.o-fprr.o|print.o|te.o|mm.o;\
l.o-fprr.o|mm.o|print.o|te.o;\
te.o-print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|schedconf.o;\
sh0.o-fprr.o|schedconf.o|print.o|[calll_]sh1.o|[callr_]sh2.o;\
sh1.o-fprr.o|schedconf.o|print.o|[calll_]sbc0.o|[callr_]sh3.o;\
sh2.o-fprr.o|schedconf.o|print.o|[calll_]sh3.o|[callr_]sbc1.o;\
sh3.o-fprr.o|schedconf.o|print.o|[calll_]sbc2.o|[callr_]sbc3.o\
" ./gen_client_stub
