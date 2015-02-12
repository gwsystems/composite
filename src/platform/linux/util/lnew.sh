#!/bin/sh

# sh? between id 18<->39, 40->50 are base cases

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpool.o,a2;!l.o,a8;!stat.o,a25;!te.o,a3;!e.o,a3;!smn.o,a2;!va.o,a1;!cbuf.o,a2;!vm.o,a1;\
\
(!p0.o=exe_cb_pt.o),a9'p5 e1500 s0 d120';\
(!p1.o=exe_cb_pt.o),a11'p7 e1700 s0 d120';\
(!p2.o=exe_cb_pt.o),a8'p4 e4600 s0 d120';\
(!p3.o=exe_cb_pt.o),a10'p6 e4400 s0 d120';\
(!p4.o=exe_cb_pt.o),a10'p6 e4400 s0 d120';\
(!p5.o=exe_cb_pt.o),a12'p15 e10400 s0 d120';\
(!p6.o=exe_cb_pt.o),a13'p18 e10400 s0 d120';\
(!p7.o=exe_cb_pt.o),a14'p20 e15000 s0 d120';\
(!p8.o=exe_cb_pt.o),a15'p33 e66000 s0 d120';\
(!p9.o=exe_cb_pt.o),a15'p33 e33000 s0 d120';\
\
(!sh0.o=exe_cb_sh.o),'s50000 n1';(!sh1.o=exe_cb_sh.o),'s5000 n1 r2';(!sh2.o=exe_cb_sh.o),'s50000 n1 r96 ';\
(!sh3.o=exe_cb_sh.o),'s5000 n1 r32';(!sh4.o=exe_cb_sh.o),'s50000 n1 r125';(!sh5.o=exe_cb_sh.o),'s50000 n1 r96 ';\
(!sh6.o=exe_cb_sh.o),'s50000 n1 r32';(!sh7.o=exe_cb_sh.o),'s50000 n1 r96';(!sh8.o=exe_cb_sh.o),'s500000 n1 ';\
\
(!sh18.o=exe_cb_sh.o),'s500000 n1 r32';(!sh19.o=exe_cb_sh.o),'s50000 n1 r32';(!sh20.o=exe_cb_sh.o),'s50000 n1 r96';\
(!sh15.o=exe_cb_sh.o),'s50000 n1 r32';(!sh16.o=exe_cb_sh.o),'s5000 n1 r32';(!sh17.o=exe_cb_sh.o),'s50000 n1 r96';\
\
!exe_cb_sbc.o, :\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
cg.o-fprr.o;\
l.o-fprr.o|mm.o|print.o|te.o|va.o;\
te.o-print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o|smn.o;\
stat.o-te.o|fprr.o|l.o|print.o|e.o|smn.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|schedconf.o|cg.o;\
vm.o-print.o|fprr.o|mm.o|boot.o;\
va.o-print.o|fprr.o|mm.o|boot.o|vm.o;\
smn.o-print.o|fprr.o|mm.o|boot.o|va.o|mpool.o|l.o;\
cbuf.o-fprr.o|print.o|l.o|mm.o|boot.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
\
\
p0.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p1.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p2.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p3.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p4.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p5.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p6.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p7.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p8.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
p9.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|smn.o;\
\
sh18.o-fprr.o|schedconf.o|print.o|[calll_]sh15.o|[callr_]sh19.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh19.o-fprr.o|schedconf.o|print.o|[calll_]sh16.o|[callr_]sh20.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh20.o-fprr.o|schedconf.o|print.o|[calll_]sh17.o|[callr_]exe_cb_sbc.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh15.o-fprr.o|schedconf.o|print.o|[calll_]sh0.o|[callr_]sh16.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh16.o-fprr.o|schedconf.o|print.o|[calll_]sh2.o|[callr_]sh17.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh17.o-fprr.o|schedconf.o|print.o|[calll_]sh5.o|[callr_]exe_cb_sbc.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
\
sh0.o-fprr.o|schedconf.o|print.o|[calll_]sh1.o|[callr_]sh2.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh1.o-fprr.o|schedconf.o|print.o|[calll_]sh3.o|[callr_]sh4.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh2.o-fprr.o|schedconf.o|print.o|[calll_]sh4.o|[callr_]sh5.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh3.o-fprr.o|schedconf.o|print.o|[calll_]exe_cb_sbc.o|[callr_]sh6.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh4.o-fprr.o|schedconf.o|print.o|[calll_]sh6.o|[callr_]sh7.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh5.o-fprr.o|schedconf.o|print.o|[calll_]sh7.o|[callr_]exe_cb_sbc.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh6.o-fprr.o|schedconf.o|print.o|[calll_]exe_cb_sbc.o|[callr_]sh8.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh7.o-fprr.o|schedconf.o|print.o|[calll_]sh8.o|[callr_]exe_cb_sbc.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
sh8.o-fprr.o|schedconf.o|print.o|[calll_]exe_cb_sbc.o|[callr_]exe_cb_sbc.o|smn.o|cbuf.o|va.o|mm.o|l.o;\
\
exe_cb_sbc.o-smn.o|cbuf.o|va.o|mm.o|print.o\
" ./gen_client_stub
