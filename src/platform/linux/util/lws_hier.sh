#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o,a2;schedconf.o, ;cg.o,a1;bc.o, ;(*fprrc1.o=fprr.o),a5;st.o, ;\
\
!stat.o,a25;!cm.o,a7;!sc.o,a6;!if.o,a5;!ip.o, ;!ainv.o,a6;!fn.o, ;!cgi.o,a9;!port.o, ;!mpd.o,a5;\
!sm.o,a1;!l.o,a4;!te.o,a3;(!fd2.o=fd.o),a8;(!fd3.o=fd.o),a8;(!cgi2.o=cgi.o),a9;(!ainv2.o=ainv.o),a6;\
!net.o,a6;!e.o,a5;!fd.o,a8;!conn.o,a9;!http.o,a8:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
fprrc1.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
net.o-sm.o|fprrc1.o|mm.o|print.o|l.o|te.o|e.o|ip.o|port.o;\
l.o-sm.o|fprrc1.o|mm.o|print.o|te.o;\
te.o-sm.o|print.o|fprrc1.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|fprrc1.o|print.o|mm.o|l.o|st.o;\
fd.o-sm.o|print.o|e.o|net.o|l.o|fprrc1.o|http.o|mm.o;\
conn.o-sm.o|fd.o|print.o|mm.o|fprrc1.o;\
http.o-sm.o|mm.o|print.o|fprrc1.o|cm.o|te.o;\
stat.o-sm.o|te.o|fprrc1.o|l.o|print.o|e.o;\
st.o-print.o;\
ip.o-sm.o|if.o;\
port.o-sm.o|l.o;\
cm.o-sm.o|print.o|mm.o|sc.o|fprrc1.o|ainv.o|[alt_]ainv2.o;\
sc.o-sm.o|print.o|mm.o|e.o|fprrc1.o;\
if.o-sm.o|print.o|mm.o|l.o|fprrc1.o;\
fn.o-sm.o|fprrc1.o;\
fd2.o-sm.o|fn.o|ainv.o|print.o|mm.o|fprrc1.o|e.o|l.o;\
ainv.o-sm.o|mm.o|print.o|fprrc1.o|l.o|e.o;\
cgi.o-sm.o|fd2.o|fprrc1.o|print.o;\
fd3.o-sm.o|fn.o|ainv2.o|print.o|mm.o|fprrc1.o|e.o|l.o;\
ainv2.o-sm.o|mm.o|print.o|fprrc1.o|l.o|e.o;\
cgi2.o-sm.o|fd3.o|fprrc1.o|print.o;\
schedconf.o-print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
sm.o-print.o|fprrc1.o|mm.o|boot.o;\
mpd.o-sm.o|cg.o|fprrc1.o|print.o|te.o|mm.o;\
cg.o-fprr.o;\
bc.o-print.o\
" ./gen_client_stub


