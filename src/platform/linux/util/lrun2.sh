#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;!sm.o,a1;print.o, ;boot.o,a2;!l.o,a8;!te.o,a3;!net.o,a6;!e.o,a3;!fd.o,a8;!conn.o,a9;!http.o,a8;\
!stat.o,a25;st.o, ;!cm.o,a7;!sc.o,a6;!if.o,a5;!ip.o, ;!ainv.o,a6;!fn.o, ;!cgi.o,a9;!port.o, ;schedconf.o, ;\
bc.o, ;(!fd2.o=fd.o),a8;(!fd3.o=fd.o),a8;(!cgi2.o=cgi.o),a9;(!ainv2.o=ainv.o),a6:\
\
net.o-sm.o|fprr.o|mm.o|print.o|l.o|te.o|e.o|ip.o|port.o;\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
l.o-sm.o|fprr.o|mm.o|print.o|te.o;\
te.o-sm.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|st.o;\
fd.o-sm.o|print.o|e.o|net.o|l.o|fprr.o|http.o|mm.o;\
conn.o-sm.o|fd.o|print.o|mm.o|fprr.o;\
http.o-sm.o|mm.o|print.o|fprr.o|cm.o|te.o;\
stat.o-sm.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
ip.o-sm.o|if.o;\
port.o-sm.o|l.o;\
cm.o-sm.o|print.o|mm.o|sc.o|fprr.o|ainv.o|[alt_]ainv2.o;\
sc.o-sm.o|print.o|mm.o|e.o|fprr.o;\
if.o-sm.o|print.o|mm.o|l.o|fprr.o;\
fn.o-sm.o|fprr.o;\
fd2.o-sm.o|fn.o|ainv.o|print.o|mm.o|fprr.o|e.o|l.o;\
ainv.o-sm.o|mm.o|print.o|fprr.o|l.o|e.o;\
cgi.o-sm.o|fd2.o|fprr.o|print.o;\
fd3.o-sm.o|fn.o|ainv2.o|print.o|mm.o|fprr.o|e.o|l.o;\
ainv2.o-sm.o|mm.o|print.o|fprr.o|l.o|e.o;\
cgi2.o-sm.o|fd3.o|fprr.o|print.o;\
schedconf.o-print.o;\
boot.o-print.o|fprr.o|mm.o;\
sm.o-print.o|fprr.o|mm.o|boot.o;\
bc.o-print.o\
" ./gen_client_stub

# mpd.o-fprr.o|print.o|te.o|mm.o;\
