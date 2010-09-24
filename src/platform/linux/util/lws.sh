#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000

./cos_loader \
"c0.o, ;*ds.o, ;mm.o, ;print.o, ;boot.o,a2;schedconf.o, ;cg.o,a1;bc.o, ;st.o, ;\
\
!sm.o,a1;!mpd.o,a5;!stat.o,a25;!cm.o,a7;!sc.o,a6;!if.o,a5;!ip.o, ;!ainv.o,a6;!fn.o, ;!cgi.o,a9;\
!port.o, ;!l.o,a4;!te.o,a3;(!fd2.o=fd.o),a8;(!fd3.o=fd.o),a8;(!cgi2.o=cgi.o),a9;(!ainv2.o=ainv.o),a6;\
!net.o,d6c2t2;!e.o,a5;!fd.o,a8;!conn.o,a9;!http.o,a8:\
\
c0.o-ds.o;\
ds.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
net.o-sm.o|ds.o|mm.o|print.o|l.o|te.o|e.o|ip.o|port.o;\
l.o-sm.o|ds.o|mm.o|print.o|te.o;\
te.o-sm.o|print.o|ds.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|ds.o|print.o|mm.o|l.o|st.o;\
fd.o-sm.o|print.o|e.o|net.o|l.o|ds.o|http.o|mm.o;\
conn.o-sm.o|fd.o|print.o|mm.o|ds.o;\
http.o-sm.o|mm.o|print.o|ds.o|cm.o|te.o;\
stat.o-sm.o|te.o|ds.o|l.o|print.o|e.o;\
st.o-print.o;\
ip.o-sm.o|if.o;\
port.o-sm.o|l.o;\
cm.o-sm.o|print.o|mm.o|sc.o|ds.o|ainv.o|[alt_]ainv2.o;\
sc.o-sm.o|print.o|mm.o|e.o|ds.o;\
if.o-sm.o|print.o|mm.o|l.o|ds.o;\
fn.o-sm.o|ds.o;\
fd2.o-sm.o|fn.o|ainv.o|print.o|mm.o|ds.o|e.o|l.o;\
ainv.o-sm.o|mm.o|print.o|ds.o|l.o|e.o;\
cgi.o-sm.o|fd2.o|ds.o|print.o;\
fd3.o-sm.o|fn.o|ainv2.o|print.o|mm.o|ds.o|e.o|l.o;\
ainv2.o-sm.o|mm.o|print.o|ds.o|l.o|e.o;\
cgi2.o-sm.o|fd3.o|ds.o|print.o;\
schedconf.o-print.o;\
boot.o-print.o|ds.o|mm.o|cg.o;\
sm.o-print.o|ds.o|mm.o|boot.o;\
mpd.o-sm.o|cg.o|ds.o|print.o|te.o|mm.o;\
cg.o-ds.o;\
bc.o-print.o\
" ./gen_client_stub


