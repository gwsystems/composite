#!/bin/sh

# "c0.o, ;*fprr.o, ;mpd.o,a5;!l.o,a8;mm.o, ;print.o, ;!te.o,a3;!e.o,a3;schedconf.o, ;\
# mpd.o-fprr.o|print.o|te.o|mm.o;\
# p1.o-te.o|fprr.o|schedconf.o|print.o|sh12.o|cbuf.o;\
# p2.o-te.o|fprr.o|schedconf.o|print.o|sh12.o|cbuf.o;\
# (!p0.o=pt.o),a7'5';(!p1.o=pt.o),a8'10';(!p2.o=pt.o),a9'12';\
# (!p3.o=pt.o),a10'18';(!p4.o=pt.o),a11'20';(!p5.o=pt.o),a12'25';\
# p4.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|cbuf.o;\
# p5.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|cbuf.o;\

# sh? between id 18<->39, 40->50 are base cases

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!l.o,a8;!stat.o,a25;!te.o,a3;!e.o,a3;!sp.o,a4;\
\
(!p0.o=pt.o),a7'p10';(!p1.o=pt.o),a8'p15';\
(!p3.o=pt.o),a9'p33 n1';(!p4.o=pt.o),a9'p33 n1';(!p5.o=pt.o),a9'p33 n1';\
(!p6.o=pt.o),a9'p33 n1';(!p7.o=pt.o),a9'p333 n1';\
\
(!sh0.o=sh.o),'s50000 n2';(!sh1.o=sh.o),'s500 n2 r32';(!sh2.o=sh.o),'s500 n2 r96';\
(!sh3.o=sh.o),'s500 n2 r32';(!sh4.o=sh.o),'s50000 n2';(!sh5.o=sh.o),'s500 n2 r96';\
(!sh6.o=sh.o),'s500 n2 r32';(!sh7.o=sh.o),'s500 n2 r96';(!sh8.o=sh.o),'s50000 n2';\
\
(!sh12.o=sh.o),'s500 n2 r32';(!sh13.o=sh.o),'s500 n2 r32';(!sh14.o=sh.o),'s500 n2 r32';\
(!sh9.o=sh.o),'s500 n2 r32';(!sh10.o=sh.o),'s500 n2 r32';(!sh11.o=sh.o),'s500 n2 r32';\
\
(!sh18.o=sh.o),'s500 n2 r96';(!sh19.o=sh.o),'s500 n2 r96';(!sh20.o=sh.o),'s500 n2 r96';\
(!sh15.o=sh.o),'s500 n2 r96';(!sh16.o=sh.o),'s500 n2 r96';(!sh17.o=sh.o),'s500 n2 r96';\
\
!sbc.o, :\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
cg.o-fprr.o;\
l.o-fprr.o|mm.o|print.o|te.o|cbuf.o;\
te.o-print.o|fprr.o|mm.o|cbuf.o;\
mm.o-print.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o|cbuf.o;\
stat.o-te.o|fprr.o|l.o|print.o|e.o|cbuf.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|schedconf.o|cg.o;\
sp.o-te.o|fprr.o|schedconf.o|print.o|mm.o|cbuf.o;\
\
\
p0.o-te.o|fprr.o|schedconf.o|print.o|sh12.o|cbuf.o;\
p1.o-te.o|fprr.o|schedconf.o|print.o|sh12.o|cbuf.o;\
p3.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|cbuf.o;\
p4.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|cbuf.o;\
p5.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|cbuf.o;\
p6.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|cbuf.o;\
p7.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|cbuf.o;\
\
sh12.o-fprr.o|schedconf.o|print.o|[calll_]sh13.o|[callr_]sh9.o|cbuf.o;\
sh13.o-fprr.o|schedconf.o|print.o|[calll_]sh14.o|[callr_]sh10.o|cbuf.o;\
sh14.o-fprr.o|schedconf.o|print.o|[calll_]sbc.o|[callr_]sh11.o|cbuf.o;\
sh9.o-fprr.o|schedconf.o|print.o|[calll_]sh10.o|[callr_]sh0.o|cbuf.o;\
sh10.o-fprr.o|schedconf.o|print.o|[calll_]sh11.o|[callr_]sh1.o|cbuf.o;\
sh11.o-fprr.o|schedconf.o|print.o|[calll_]sbc.o|[callr_]sh3.o|cbuf.o;\
\
sh18.o-fprr.o|schedconf.o|print.o|[calll_]sh15.o|[callr_]sh19.o|cbuf.o;\
sh19.o-fprr.o|schedconf.o|print.o|[calll_]sh16.o|[callr_]sh20.o|cbuf.o;\
sh20.o-fprr.o|schedconf.o|print.o|[calll_]sh17.o|[callr_]sbc.o|cbuf.o;\
sh15.o-fprr.o|schedconf.o|print.o|[calll_]sh0.o|[callr_]sh16.o|cbuf.o;\
sh16.o-fprr.o|schedconf.o|print.o|[calll_]sh2.o|[callr_]sh17.o|cbuf.o;\
sh17.o-fprr.o|schedconf.o|print.o|[calll_]sh5.o|[callr_]sbc.o|cbuf.o;\
\
sh0.o-fprr.o|schedconf.o|print.o|[calll_]sh1.o|[callr_]sh2.o|cbuf.o;\
sh1.o-fprr.o|schedconf.o|print.o|[calll_]sh3.o|[callr_]sh4.o|cbuf.o;\
sh2.o-fprr.o|schedconf.o|print.o|[calll_]sh4.o|[callr_]sh5.o|cbuf.o;\
sh3.o-fprr.o|schedconf.o|print.o|[calll_]sbc.o|[callr_]sh6.o|cbuf.o;\
sh4.o-fprr.o|schedconf.o|print.o|[calll_]sh6.o|[callr_]sh7.o|cbuf.o;\
sh5.o-fprr.o|schedconf.o|print.o|[calll_]sh7.o|[callr_]sbc.o|cbuf.o;\
sh6.o-fprr.o|schedconf.o|print.o|[calll_]sbc.o|[callr_]sh8.o|cbuf.o;\
sh7.o-fprr.o|schedconf.o|print.o|[calll_]sh8.o|[callr_]sbc.o|cbuf.o;\
sh8.o-fprr.o|schedconf.o|print.o|[calll_]sbc.o|[callr_]sbc.o|cbuf.o;\
\
sbc.o-cbuf.o\
" ./gen_client_stub
