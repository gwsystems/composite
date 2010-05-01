#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mpd.o,a4;!l.o,a8;mm.o, ;print.o, ;te.o,a3;!e.o,a3;schedconf.o, ;!stat.o,a25;st.o, ;bc.o, ;\
boot.o,a4;\
(!sh0.o=sh.o),'s10000 n10';(!sh1.o=sh.o),'s10000 n10';(!sh2.o=sh.o),'s10000 n10';\
(!sh3.o=sh.o),'s10000 n10';(!sh4.o=sh.o),'s10000 n10';(!sh5.o=sh.o),'s10000 n10';\
(!sh6.o=sh.o),'s10000 n10';(!sh7.o=sh.o),'s10000 n10';(!sh8.o=sh.o),'s10000 n10';\
(!sbc0.o=sbc.o), ;(!sbc1.o=sbc.o), ;(!sbc2.o=sbc.o), ;\
(!sbc3.o=sbc.o), ;(!sbc4.o=sbc.o), ;(!sbc5.o=sbc.o), ;\
\
(!sh12.o=sh.o),a10's10000 n10';(!sh13.o=sh.o),'s10000 n10';(!sh14.o=sh.o),'s10000 n10';\
(!sh9.o=sh.o),'s10000 n10';(!sh10.o=sh.o),'s10000 n10';(!sh11.o=sh.o),'s10000 n10';\
(!sbc6.o=sbc.o), ;(!sbc7.o=sbc.o), ;\
\
(!sh18.o=sh.o),a10's10000 n10';(!sh19.o=sh.o),'s10000 n10';(!sh20.o=sh.o),'s10000 n10';\
(!sh15.o=sh.o),'s10000 n10';(!sh16.o=sh.o),'s10000 n10';(!sh17.o=sh.o),'s10000 n10';\
(!sbc8.o=sbc.o), ;(!sbc9.o=sbc.o), :\
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
\
sh12.o-fprr.o|schedconf.o|print.o|[calll_]sh13.o|[callr_]sh9.o;\
sh13.o-fprr.o|schedconf.o|print.o|[calll_]sh14.o|[callr_]sh10.o;\
sh14.o-fprr.o|schedconf.o|print.o|[calll_]sbc7.o|[callr_]sh11.o;\
sh9.o-fprr.o|schedconf.o|print.o|[calll_]sh10.o|[callr_]sh0.o;\
sh10.o-fprr.o|schedconf.o|print.o|[calll_]sh11.o|[callr_]sh1.o;\
sh11.o-fprr.o|schedconf.o|print.o|[calll_]sbc6.o|[callr_]sh3.o;\
\
sh18.o-fprr.o|schedconf.o|print.o|[calll_]sh15.o|[callr_]sh19.o;\
sh19.o-fprr.o|schedconf.o|print.o|[calll_]sh16.o|[callr_]sh20.o;\
sh20.o-fprr.o|schedconf.o|print.o|[calll_]sh17.o|[callr_]sbc9.o;\
sh15.o-fprr.o|schedconf.o|print.o|[calll_]sh0.o|[callr_]sh16.o;\
sh16.o-fprr.o|schedconf.o|print.o|[calll_]sh2.o|[callr_]sh17.o;\
sh17.o-fprr.o|schedconf.o|print.o|[calll_]sh5.o|[callr_]sbc8.o;\
\
sh0.o-fprr.o|schedconf.o|print.o|[calll_]sh1.o|[callr_]sh2.o;\
sh1.o-fprr.o|schedconf.o|print.o|[calll_]sh3.o|[callr_]sh4.o;\
sh2.o-fprr.o|schedconf.o|print.o|[calll_]sh4.o|[callr_]sh5.o;\
sh3.o-fprr.o|schedconf.o|print.o|[calll_]sbc0.o|[callr_]sh6.o;\
sh4.o-fprr.o|schedconf.o|print.o|[calll_]sh6.o|[callr_]sh7.o;\
sh5.o-fprr.o|schedconf.o|print.o|[calll_]sh7.o|[callr_]sbc5.o;\
sh6.o-fprr.o|schedconf.o|print.o|[calll_]sbc1.o|[callr_]sh8.o;\
sh7.o-fprr.o|schedconf.o|print.o|[calll_]sh8.o|[callr_]sbc4.o;\
sh8.o-fprr.o|schedconf.o|print.o|[calll_]sbc2.o|[callr_]sbc3.o\
" ./gen_client_stub
