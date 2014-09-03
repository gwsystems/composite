#!/bin/sh

# sh? between id 18<->39, 40->50 are base cases

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;boot.o,a4;cg.o,a1;pf.o, ;\
\
!l.o,a8;!stat.o,a25;!te.o,a3;!e.o,a3;!smn.o,a2;!va.o,a1;!sp.o,a4;!vm.o, ;\
\
(!p0.o=exe_pt.o),a9'p5 e500 s0 d120 m1';\
(!p1.o=exe_pt.o),a11'p7 e700 s0 d120 m1';\
(!p2.o=exe_pt.o),a8'p4 e3600 s0 d120 m1';\
(!p3.o=exe_pt.o),a10'p6 e2400 s0 d120 m1';\
(!p4.o=exe_pt.o),a10'p6 e600 s0 d120 m1';\
(!p5.o=exe_pt.o),a7'p3 e1500 s0 d120 m1';\
(!p6.o=exe_pt.o),a7'p3 e1800 s0 d120 m1';\
(!p7.o=exe_pt.o),a12'p13 e10400 s0 d120 m1';\
(!p8.o=exe_pt.o),a14'p29 e2900 s0 d120 m1';\
(!p9.o=exe_pt.o),a13'p17 e6800 s0 d120 m1';\
\
(!sh0.o=exe_sh.o),'s50000 n2 a0';(!sh1.o=exe_sh.o),'s5000 n2 r2 a0';(!sh2.o=exe_sh.o),'s50000 n2 r96 a0';\
(!sh3.o=exe_sh.o),'s5000 n2 r32 a1';(!sh4.o=exe_sh.o),'s50000 n2 r125 a0';(!sh5.o=exe_sh.o),'s50000 n2 r96 a0';\
(!sh6.o=exe_sh.o),'s50000 n2 r32 a0';(!sh7.o=exe_sh.o),'s50000 n2 r96 a0';(!sh8.o=exe_sh.o),'s500000 n2 a0';\
\
(!sh12.o=exe_sh.o),'s500000 n2 r32 a0';(!sh13.o=exe_sh.o),'s50000 n2 r32 a0';(!sh14.o=exe_sh.o),'s50000 n2 r32 a0';\
(!sh9.o=exe_sh.o),'s50000 n2 r32 a0';(!sh10.o=exe_sh.o),'s500000 n2 r32 a0';(!sh11.o=exe_sh.o),'s50000 n2 r32 a0';\
\
(!sh18.o=exe_sh.o),'s500000 n2 r32 a0';(!sh19.o=exe_sh.o),'s50000 n2 r32 a0';(!sh20.o=exe_sh.o),'s50000 n2 r96 a0';\
(!sh15.o=exe_sh.o),'s50000 n2 r32 a0';(!sh16.o=exe_sh.o),'s5000 n2 r32 a0';(!sh17.o=exe_sh.o),'s50000 n2 r96 a0';\
\
!exe_sbc.o, :\
\
c0.o-fprr.o;\
pf.o-print.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o|pf.o;\
cg.o-fprr.o;\
l.o-fprr.o|mm.o|print.o|te.o|smn.o|va.o;\
te.o-print.o|fprr.o|mm.o|smn.o|va.o;\
mm.o-print.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o|smn.o;\
stat.o-te.o|fprr.o|l.o|print.o|e.o|smn.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
boot.o-print.o|fprr.o|mm.o|schedconf.o|cg.o;\
vm.o-print.o|fprr.o|mm.o|boot.o;\
va.o-print.o|fprr.o|mm.o|boot.o|vm.o;\
sp.o-te.o|fprr.o|schedconf.o|print.o|mm.o|smn.o;\
smn.o-print.o|fprr.o|mm.o|boot.o|va.o;\
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
sh12.o-fprr.o|schedconf.o|print.o|[calll_]sh13.o|[callr_]sh9.o|smn.o;\
sh13.o-fprr.o|schedconf.o|print.o|[calll_]sh14.o|[callr_]sh10.o|smn.o;\
sh14.o-fprr.o|schedconf.o|print.o|[calll_]exe_sbc.o|[callr_]sh11.o|smn.o;\
sh9.o-fprr.o|schedconf.o|print.o|[calll_]sh10.o|[callr_]sh0.o|smn.o;\
sh10.o-fprr.o|schedconf.o|print.o|[calll_]sh11.o|[callr_]sh1.o|smn.o;\
sh11.o-fprr.o|schedconf.o|print.o|[calll_]exe_sbc.o|[callr_]sh3.o|smn.o;\
\
sh18.o-fprr.o|schedconf.o|print.o|[calll_]sh15.o|[callr_]sh19.o|smn.o;\
sh19.o-fprr.o|schedconf.o|print.o|[calll_]sh16.o|[callr_]sh20.o|smn.o;\
sh20.o-fprr.o|schedconf.o|print.o|[calll_]sh17.o|[callr_]exe_sbc.o|smn.o;\
sh15.o-fprr.o|schedconf.o|print.o|[calll_]sh0.o|[callr_]sh16.o|smn.o;\
sh16.o-fprr.o|schedconf.o|print.o|[calll_]sh2.o|[callr_]sh17.o|smn.o;\
sh17.o-fprr.o|schedconf.o|print.o|[calll_]sh5.o|[callr_]exe_sbc.o|smn.o;\
\
sh0.o-fprr.o|schedconf.o|print.o|[calll_]sh1.o|[callr_]sh2.o|smn.o;\
sh1.o-fprr.o|schedconf.o|print.o|[calll_]sh3.o|[callr_]sh4.o|smn.o;\
sh2.o-fprr.o|schedconf.o|print.o|[calll_]sh4.o|[callr_]sh5.o|smn.o;\
sh3.o-fprr.o|schedconf.o|print.o|[calll_]exe_sbc.o|[callr_]sh6.o|smn.o;\
sh4.o-fprr.o|schedconf.o|print.o|[calll_]sh6.o|[callr_]sh7.o|smn.o;\
sh5.o-fprr.o|schedconf.o|print.o|[calll_]sh7.o|[callr_]exe_sbc.o|smn.o;\
sh6.o-fprr.o|schedconf.o|print.o|[calll_]exe_sbc.o|[callr_]sh8.o|smn.o;\
sh7.o-fprr.o|schedconf.o|print.o|[calll_]sh8.o|[callr_]exe_sbc.o|smn.o;\
sh8.o-fprr.o|schedconf.o|print.o|[calll_]exe_sbc.o|[callr_]exe_sbc.o|smn.o;\
\
exe_sbc.o-smn.o\
" ./gen_client_stub
