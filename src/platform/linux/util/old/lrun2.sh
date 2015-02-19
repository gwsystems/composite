#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o,a2;!l.o,a8;!te.o,a3;!net.o,a6;!e.o,a3;!fd.o,a8;!conn.o,a9;!http.o,a8;\
!stat.o,a25;st.o, ;!cm.o,a7;!sc.o,a6;!if.o,a5;!ip.o, ;!ainv.o,a6;!fn.o, ;!cgi.o,a9;!port.o, ;schedconf.o, ;\
(*fprrc1.o=fprr.o),a11;bc.o, ;(!fd2.o=fd.o),a8;(!fd3.o=fd.o),a8;(!cgi2.o=cgi.o),a9;(!ainv2.o=ainv.o),a6:\
\
net.o-cbuf.o|fprr.o|mm.o|print.o|l.o|te.o|e.o|ip.o|port.o;\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
fprrc1.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
l.o-cbuf.o|fprr.o|mm.o|print.o|te.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|st.o;\
fd.o-cbuf.o|print.o|e.o|net.o|l.o|fprr.o|http.o|mm.o;\
conn.o-cbuf.o|fd.o|print.o|mm.o|fprr.o;\
http.o-cbuf.o|mm.o|print.o|fprr.o|cm.o|te.o;\
stat.o-cbuf.o|te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
ip.o-cbuf.o|if.o;\
port.o-cbuf.o|l.o;\
cm.o-cbuf.o|print.o|mm.o|sc.o|fprr.o|ainv.o|[alt_]ainv2.o;\
sc.o-cbuf.o|print.o|mm.o|e.o|fprr.o;\
if.o-cbuf.o|print.o|mm.o|l.o|fprr.o;\
fn.o-cbuf.o|fprr.o;\
fd2.o-cbuf.o|fn.o|ainv.o|print.o|mm.o|fprr.o|e.o|l.o;\
ainv.o-cbuf.o|mm.o|print.o|fprr.o|l.o|e.o;\
cgi.o-cbuf.o|fd2.o|fprr.o|print.o;\
fd3.o-cbuf.o|fn.o|ainv2.o|print.o|mm.o|fprr.o|e.o|l.o;\
ainv2.o-cbuf.o|mm.o|print.o|fprr.o|l.o|e.o;\
cgi2.o-cbuf.o|fd3.o|fprr.o|print.o;\
schedconf.o-print.o;\
boot.o-print.o|fprr.o|mm.o;\
\
bc.o-print.o\
" ./gen_client_stub

# mpd.o-fprr.o|print.o|te.o|mm.o;\
