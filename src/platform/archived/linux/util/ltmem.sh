#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;boot.o, ;print.o, ;\
\
!l.o,a1;!va.o,a2;!mpool.o,a3;!te.o,a3;!e.o,a4;!cbuf.o,a5;!stat.o,a25;!vm.o.a1;\
\
(!p0.o=exe_cb_pt.o),a10'a10 p3 e3000 s0 d120';\
(!p1.o=exe_cb_pt.o),a11'a11 p4 e4000 s0 d120';\
(!p2.o=exe_cb_pt.o),a12'a12 p5 e5000 s0 d120';\
(!p3.o=exe_cb_pt.o),a13'a13 p6 e6000 s0 d120';\
(!p4.o=exe_cb_pt.o),a14'a14 p7 e7000 s0 d120';\
(!p5.o=exe_cb_pt.o),a15'a15 p8 e8000 s0 d120';\
(!p6.o=exe_cb_pt.o),a16'a16 p9 e9000 s0 d120';\
(!p7.o=exe_cb_pt.o),a17'a17 p50 e100000 s0 d120';\
\
(!sh0.o=exe_cb_sh.o),'s50000 n20000 a0';(!sh1.o=exe_cb_sh.o),'s5000 n20000 r96 a0';(!sh2.o=exe_cb_sh.o),'s50000 n20000 r32 a0';\
(!sh3.o=exe_cb_sh.o),'s5000 n20000 r96 a0';(!sh4.o=exe_cb_sh.o),'s50000 n20000 r125 a0';(!sh5.o=exe_cb_sh.o),'s50000 n20000 r32 a0';\
(!sh6.o=exe_cb_sh.o),'s50000 n20000 r96 a0';(!sh7.o=exe_cb_sh.o),'s50000 n20000 r32 a0';(!sh8.o=exe_cb_sh.o),'s500000 n20000 a0';\
\
(!sh9.o=exe_cb_sh.o),'s50000 n20000 a0 r32';(!sh10.o=exe_cb_sh.o),'s5000 n20000 r32 a0';(!sh11.o=exe_cb_sh.o),'s50000 n20000 r32 a0';\
(!sh12.o=exe_cb_sh.o),'s5000 n20000 r32 a0';(!sh13.o=exe_cb_sh.o),'s50000 n20000 r32 a0';(!sh14.o=exe_cb_sh.o),'s50000 n20000 r32 a0';\
(!sh15.o=exe_cb_sh.o),'s50000 n20000 r32 a0';(!sh16.o=exe_cb_sh.o),'s50000 n20000 r32 a0';(!sh17.o=exe_cb_sh.o),'s500000 n20000 r32 a0';\
\
(!sh18.o=exe_cb_sh.o),'s50000 n20000 a0 r96';(!sh19.o=exe_cb_sh.o),'s5000 n20000 r96 a0';(!sh20.o=exe_cb_sh.o),'s50000 n20000 r96 a0';\
(!sh21.o=exe_cb_sh.o),'s5000 n20000 r96 a0';(!sh22.o=exe_cb_sh.o),'s50000 n20000 r96 a0';(!sh23.o=exe_cb_sh.o),'s50000 n20000 r96 a0';\
(!sh24.o=exe_cb_sh.o),'s50000 n20000 r96 a0';(!sh25.o=exe_cb_sh.o),'s50000 n20000 r96 a0';(!sh26.o=exe_cb_sh.o),'s500000 n20000 r96 a0';\
\
!exe_cb_sbc.o, :\
\
c0.o-fprr.o;\
fprr.o-print.o|[parent_]mm.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|va.o;\
stat.o-cbuf.o|te.o|fprr.o|l.o|print.o|e.o;\
boot.o-print.o|fprr.o|mm.o;\
\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
\
p0.o-te.o|fprr.o|print.o|sh9.o|cbuf.o|va.o|mm.o;\
p1.o-te.o|fprr.o|print.o|sh9.o|cbuf.o|va.o|mm.o;\
p2.o-te.o|fprr.o|print.o|sh9.o|cbuf.o|va.o|mm.o;\
p3.o-te.o|fprr.o|print.o|sh9.o|cbuf.o|va.o|mm.o;\
p4.o-te.o|fprr.o|print.o|sh18.o|cbuf.o|va.o|mm.o;\
p5.o-te.o|fprr.o|print.o|sh18.o|cbuf.o|va.o|mm.o;\
p6.o-te.o|fprr.o|print.o|sh18.o|cbuf.o|va.o|mm.o;\
p7.o-te.o|fprr.o|print.o|sh18.o|cbuf.o|va.o|mm.o;\
\
sh9.o-fprr.o||print.o|[calll_]sh10.o|[callr_]sh11.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh10.o-fprr.o||print.o|[calll_]sh12.o|[callr_]sh13.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh11.o-fprr.o||print.o|[calll_]sh13.o|[callr_]sh14.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh12.o-fprr.o||print.o|[calll_]sh0.o|[callr_]sh15.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh13.o-fprr.o||print.o|[calll_]sh15.o|[callr_]sh16.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh14.o-fprr.o||print.o|[calll_]sh16.o|[callr_]exe_cb_sbc.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh15.o-fprr.o||print.o|[calll_]sh2.o|[callr_]sh17.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh16.o-fprr.o||print.o|[calll_]sh17.o|[callr_]exe_cb_sbc.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh17.o-fprr.o||print.o|[calll_]sh5.o|[callr_]exe_cb_sbc.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh18.o-fprr.o||print.o|[calll_]sh19.o|[callr_]sh20.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh19.o-fprr.o||print.o|[calll_]sh21.o|[callr_]sh22.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh20.o-fprr.o||print.o|[calll_]sh22.o|[callr_]sh23.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh21.o-fprr.o||print.o|[calll_]exe_cb_sbc.o|[callr_]sh24.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh22.o-fprr.o||print.o|[calll_]sh24.o|[callr_]sh25.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh23.o-fprr.o||print.o|[calll_]sh25.o|[callr_]sh0.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh24.o-fprr.o||print.o|[calll_]exe_cb_sbc.o|[callr_]sh26.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh25.o-fprr.o||print.o|[calll_]sh26.o|[callr_]sh1.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh26.o-fprr.o||print.o|[calll_]exe_cb_sbc.o|[callr_]sh3.o|cbuf.o|va.o|mm.o|l.o|te.o;\
\
sh0.o-fprr.o||print.o|[calll_]sh1.o|[callr_]sh2.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh1.o-fprr.o||print.o|[calll_]sh3.o|[callr_]sh4.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh2.o-fprr.o||print.o|[calll_]sh4.o|[callr_]sh5.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh3.o-fprr.o||print.o|[calll_]exe_cb_sbc.o|[callr_]sh6.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh4.o-fprr.o||print.o|[calll_]sh6.o|[callr_]sh7.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh5.o-fprr.o||print.o|[calll_]sh7.o|[callr_]exe_cb_sbc.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh6.o-fprr.o||print.o|[calll_]exe_cb_sbc.o|[callr_]sh8.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh7.o-fprr.o||print.o|[calll_]sh8.o|[callr_]exe_cb_sbc.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh8.o-fprr.o||print.o|[calll_]exe_cb_sbc.o|[callr_]exe_cb_sbc.o|cbuf.o|va.o|mm.o|l.o|te.o;\
\
exe_cb_sbc.o-cbuf.o|va.o|mm.o|print.o\
" ./gen_client_stub

#mpd.o-cbuf.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
#!mpd.o,a5;
#[print_]trans.o
