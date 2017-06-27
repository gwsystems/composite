#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000
# --hog --num-conns=500000 --rate=4700 --burst-length=50

./cos_loader \
"c0.o, ;*ds.o, ;mm.o, ;mh.o, ;print.o, ;boot.o,a2;schedconf.o, ;cg.o,a1;bc.o, ;st.o, ;\
\
!mpd.o,a5;\
\
(*fprrc1.o=fprr.o),d7c5t4;\
\
!stat.o,a25;!cm.o,a7;!sc.o,a6;!if.o,a5;!ip.o, ;!ainv.o,a6;!fn.o, ;!cgi.o,a9;\
!port.o, ;!l.o,a4;!te.o,a3;(!fd2.o=fd.o),a8;(!fd3.o=fd.o),a8;(!cgi2.o=cgi.o),a9;(!ainv2.o=ainv.o),a6;\
!net.o,a6;!e.o,a5;!fd.o,a8;!conn.o,a9;!http.o,a8;!cpu.o,d10c4t25;(!cpu2.o=cpu.o),d9c3t20;(!cpu3.o=cpu.o),d8c1t10:\
\
c0.o-ds.o;\
cpu.o-ds.o|cbuf.o;\
cpu2.o-ds.o|cbuf.o;\
cpu3.o-ds.o|cbuf.o;\
ds.o-print.o|mh.o|st.o|schedconf.o|[parent_]bc.o;\
fprrc1.o-print.o|mh.o|st.o|schedconf.o|[parent_]ds.o;\
net.o-cbuf.o|fprrc1.o|mh.o|print.o|l.o|te.o|e.o|ip.o|port.o;\
l.o-cbuf.o|fprrc1.o|mh.o|print.o|te.o;\
te.o-cbuf.o|print.o|fprrc1.o|mh.o;\
mm.o-print.o;\
mh.o-[parent_]mm.o|[main_]mm.o|print.o;\
e.o-cbuf.o|fprrc1.o|print.o|mh.o|l.o|st.o;\
fd.o-cbuf.o|print.o|e.o|net.o|l.o|fprrc1.o|http.o|mh.o;\
conn.o-cbuf.o|fd.o|print.o|mh.o|fprrc1.o;\
http.o-cbuf.o|mh.o|print.o|fprrc1.o|cm.o|te.o;\
stat.o-cbuf.o|te.o|fprrc1.o|l.o|print.o|e.o;\
st.o-print.o;\
ip.o-cbuf.o|if.o;\
port.o-cbuf.o|l.o;\
cm.o-cbuf.o|print.o|mh.o|sc.o|fprrc1.o|ainv.o|[alt_]ainv2.o;\
sc.o-cbuf.o|print.o|mh.o|e.o|fprrc1.o;\
if.o-cbuf.o|print.o|mh.o|l.o|fprrc1.o;\
fn.o-cbuf.o|fprrc1.o;\
fd2.o-cbuf.o|fn.o|ainv.o|print.o|mh.o|fprrc1.o|e.o|l.o;\
ainv.o-cbuf.o|mh.o|print.o|fprrc1.o|l.o|e.o;\
cgi.o-cbuf.o|fd2.o|fprrc1.o|print.o;\
fd3.o-cbuf.o|fn.o|ainv2.o|print.o|mh.o|fprrc1.o|e.o|l.o;\
ainv2.o-cbuf.o|mh.o|print.o|fprrc1.o|l.o|e.o;\
cgi2.o-cbuf.o|fd3.o|fprrc1.o|print.o;\
schedconf.o-print.o;\
boot.o-print.o|ds.o|mm.o|cg.o;\
\
mpd.o-cbuf.o|cg.o|ds.o|print.o|te.o|mh.o;\
cg.o-ds.o;\
bc.o-print.o\
" ./gen_client_stub


