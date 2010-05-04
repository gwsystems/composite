#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mpd.o,a4;!l.o,a8;mm.o, ;print.o, ;te.o,a3;!e.o,a3;schedconf.o, ;!stat.o,a25;st.o, ;bc.o, ;\
boot.o,a4;sp.o,a5;\
\
(!p0.o=pt.o),a7'5';(!p1.o=pt.o),a8'10';(!p2.o=pt.o),a9'12';\
(!p3.o=pt.o),a10'18';(!p4.o=pt.o),a11'20';(!p5.o=pt.o),a12'25';\
\
(!sh0.o=sh.o),'s10000 n5';(!sh1.o=sh.o),'s10000 n5';(!sh2.o=sh.o),'s10000 n5';\
(!sh3.o=sh.o),'s10000 n5';(!sh4.o=sh.o),'s10000 n5';(!sh5.o=sh.o),'s10000 n5';\
(!sh6.o=sh.o),'s10000 n5';(!sh7.o=sh.o),'s10000 n5';(!sh8.o=sh.o),'s10000 n5';\
(!sbc0.o=sbc.o), ;(!sbc1.o=sbc.o), ;(!sbc2.o=sbc.o), ;\
(!sbc3.o=sbc.o), ;(!sbc4.o=sbc.o), ;(!sbc5.o=sbc.o), ;\
\
(!sh12.o=sh.o),'s10000 n5';(!sh13.o=sh.o),'s10000 n5';(!sh14.o=sh.o),'s10000 n5';\
(!sh9.o=sh.o),'s10000 n5';(!sh10.o=sh.o),'s10000 n5';(!sh11.o=sh.o),'s10000 n5';\
(!sbc6.o=sbc.o), ;(!sbc7.o=sbc.o), ;\
\
(!sh18.o=sh.o),'s10000 n5';(!sh19.o=sh.o),'s10000 n5';(!sh20.o=sh.o),'s10000 n5';\
(!sh15.o=sh.o),'s10000 n5';(!sh16.o=sh.o),'s10000 n5';(!sh17.o=sh.o),'s10000 n5';\
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
sp.o-te.o|fprr.o|schedconf.o|print.o|mm.o;\
\
p0.o-te.o|fprr.o|schedconf.o|print.o|sh12.o;\
p1.o-te.o|fprr.o|schedconf.o|print.o|sh12.o;\
p2.o-te.o|fprr.o|schedconf.o|print.o|sh12.o;\
p3.o-te.o|fprr.o|schedconf.o|print.o|sh18.o;\
p4.o-te.o|fprr.o|schedconf.o|print.o|sh18.o;\
p5.o-te.o|fprr.o|schedconf.o|print.o|sh18.o;\
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
