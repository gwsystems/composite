#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;boot.o, ;print.o, ;\
\
!mpool.o,a3;!trans.o,a6;!l.o,a1;!te.o,a3;!e.o,a4;!stat.o,a25;!cbuf.o,a5;!va.o,a2;!vm.o,a1\
\
(!p0.o=exe_cb_pt.o),a10'p3 e600 s0 d120';\
\
(!sh0.o=exe_cb_sh.o),'s50000 n20000 a0';(!sh1.o=exe_cb_sh.o),'s5000 n20000 r2 a0';(!sh2.o=exe_cb_sh.o),'s50000 n20000 r96 a0';\
(!sh3.o=exe_cb_sh.o),'s5000 n20000 r32 a0';(!sh4.o=exe_cb_sh.o),'s50000 n20000 r125 a0';(!sh5.o=exe_cb_sh.o),'s50000 n20000 r96 a0';\
(!sh6.o=exe_cb_sh.o),'s50000 n20000 r32 a0';(!sh7.o=exe_cb_sh.o),'s50000 n20000 r96 a0';(!sh8.o=exe_cb_sh.o),'s500000 n20000 a0';\
\
(!sh9.o=exe_cb_sh.o),'s50000 n20000 r96 a0';\
(!sh10.o=exe_cb_sh.o),'s50000 n20000 r96 a0';\
(!sh11.o=exe_cb_sh.o),'s50000 n20000 r96 a0';\
(!sh12.o=exe_cb_sh.o),'s50000 n20000 r96 a0';\
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
trans.o-fprr.o|l.o|cbuf.o|mm.o|va.o|e.o|print.o;\
\
p0.o-te.o|fprr.o|print.o|sh9.o|cbuf.o|va.o|mm.o;\
\
sh9.o-fprr.o||print.o|[calll_]sh10.o|[callr_]sh11.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh10.o-fprr.o||print.o|[calll_]exe_cb_sbc.o|[callr_]sh12.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh11.o-fprr.o||print.o|[calll_]sh12.o|[callr_]sh0.o|cbuf.o|va.o|mm.o|l.o|te.o;\
sh12.o-fprr.o||print.o|[calll_]exe_cb_sbc.o|[callr_]sh1.o|cbuf.o|va.o|mm.o|l.o|te.o;\
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

#rfs.o-fprr.o|print.o|mm.o|cbuf.o|l.o|e.o|va.o;\
#tt.o-fprr.o|rfs.o|cbuf.o|mm.o|e.o|va.o|l.o|print.o;\
