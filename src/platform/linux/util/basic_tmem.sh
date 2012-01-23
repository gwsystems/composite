#!/bin/sh

# sh? between id 18<->39, 40->50 are base cases

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpool.o,a2;!l.o,a7;!stat.o,a25;!te.o,a3;!e.o,a3;!smn.o,a2;!va.o,a1;!buf.o,a2;!tp.o,a4;\
\
# define utilization
\
(!sh0.o=exe_cb_sh.o),'s50000 n2 a0';(!sh1.o=exe_cb_sh.o),'s5000 n2 r2 a0';(!sh2.o=exe_cb_sh.o),'s50000 n2 r96 a0';\
(!sh3.o=exe_cb_sh.o),'s5000 n2 r32 a1';(!sh4.o=exe_cb_sh.o),'s50000 n2 r125 a0';(!sh5.o=exe_cb_sh.o),'s50000 n2 r96 a0';\
(!sh6.o=exe_cb_sh.o),'s50000 n2 r32 a0';(!sh7.o=exe_cb_sh.o),'s50000 n2 r96 a0';(!sh8.o=exe_cb_sh.o),'s500000 n2 a0';\
\
(!sh12.o=exe_cb_sh.o),'s500000 n2 r32 a0';(!sh13.o=exe_cb_sh.o),'s50000 n2 r32 a0';(!sh14.o=exe_cb_sh.o),'s50000 n2 r32 a0';\
(!sh9.o=exe_cb_sh.o),'s50000 n2 r32 a0';(!sh10.o=exe_cb_sh.o),'s500000 n2 r32 a0';(!sh11.o=exe_cb_sh.o),'s50000 n2 r32 a0';\
\
(!sh18.o=exe_cb_sh.o),'s500000 n2 r32 a0';(!sh19.o=exe_cb_sh.o),'s50000 n2 r32 a0';(!sh20.o=exe_cb_sh.o),'s50000 n2 r96 a0';\
(!sh15.o=exe_cb_sh.o),'s50000 n2 r32 a0';(!sh16.o=exe_cb_sh.o),'s5000 n2 r32 a0';(!sh17.o=exe_cb_sh.o),'s50000 n2 r96 a0';\
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
va.o-print.o|fprr.o|mm.o|boot.o;\
smn.o-print.o|fprr.o|mm.o|boot.o|va.o|mpool.o|l.o;\
buf.o-fprr.o|print.o|l.o|mm.o|boot.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
tp.o-smn.o|buf.o|print.o|te.o|fprr.o|schedconf.o|mm.o|va.o|mpool.o;\
\
# define threads
\
sh12.o-fprr.o|schedconf.o|print.o|[calll_]sh13.o|[callr_]sh9.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh13.o-fprr.o|schedconf.o|print.o|[calll_]sh14.o|[callr_]sh10.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh14.o-fprr.o|schedconf.o|print.o|[calll_]exe_cb_sbc.o|[callr_]sh11.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh9.o-fprr.o|schedconf.o|print.o|[calll_]sh10.o|[callr_]sh0.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh10.o-fprr.o|schedconf.o|print.o|[calll_]sh11.o|[callr_]sh1.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh11.o-fprr.o|schedconf.o|print.o|[calll_]exe_cb_sbc.o|[callr_]sh3.o|smn.o|buf.o|va.o|mm.o|l.o;\
\
sh18.o-fprr.o|schedconf.o|print.o|[calll_]sh15.o|[callr_]sh19.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh19.o-fprr.o|schedconf.o|print.o|[calll_]sh16.o|[callr_]sh20.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh20.o-fprr.o|schedconf.o|print.o|[calll_]sh17.o|[callr_]exe_cb_sbc.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh15.o-fprr.o|schedconf.o|print.o|[calll_]sh0.o|[callr_]sh16.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh16.o-fprr.o|schedconf.o|print.o|[calll_]sh2.o|[callr_]sh17.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh17.o-fprr.o|schedconf.o|print.o|[calll_]sh5.o|[callr_]exe_cb_sbc.o|smn.o|buf.o|va.o|mm.o|l.o;\
\
sh0.o-fprr.o|schedconf.o|print.o|[calll_]sh1.o|[callr_]sh2.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh1.o-fprr.o|schedconf.o|print.o|[calll_]sh3.o|[callr_]sh4.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh2.o-fprr.o|schedconf.o|print.o|[calll_]sh4.o|[callr_]sh5.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh3.o-fprr.o|schedconf.o|print.o|[calll_]exe_cb_sbc.o|[callr_]sh6.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh4.o-fprr.o|schedconf.o|print.o|[calll_]sh6.o|[callr_]sh7.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh5.o-fprr.o|schedconf.o|print.o|[calll_]sh7.o|[callr_]exe_cb_sbc.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh6.o-fprr.o|schedconf.o|print.o|[calll_]exe_cb_sbc.o|[callr_]sh8.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh7.o-fprr.o|schedconf.o|print.o|[calll_]sh8.o|[callr_]exe_cb_sbc.o|smn.o|buf.o|va.o|mm.o|l.o;\
sh8.o-fprr.o|schedconf.o|print.o|[calll_]exe_cb_sbc.o|[callr_]exe_cb_sbc.o|smn.o|buf.o|va.o|mm.o|l.o;\
\
exe_cb_sbc.o-smn.o|va.o|mm.o|print.o\
" ./gen_client_stub
