#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000

./cos_loader \
"c0.o, ;*ds.o, ;mm.o, ;mh.o, ;print.o, ;boot.o,a2;schedconf.o, ;cg.o,a1;bc.o, ;st.o, ;\
\
!mpd.o,a5;!stat.o,a25;!cm.o,a7;!if.o,a5;!ip.o, ;\
!port.o, ;!l.o,a4;!te.o,a3;!net.o,d6c2t2;!e.o,a5;!fd.o,a8;!conn.o,a9;!va.o,a2;!echo.o,a8;!vm.o,a1:\
\
c0.o-ds.o;\
ds.o-print.o|mh.o|st.o|schedconf.o|[parent_]bc.o;\
net.o-cbuf.o|ds.o|mh.o|print.o|l.o|te.o|e.o|ip.o|port.o|va.o;\
l.o-ds.o|mh.o|print.o;\
te.o-cbuf.o|print.o|ds.o|mh.o|va.o;\
mm.o-print.o;\
mh.o-[parent_]mm.o|[main_]mm.o|print.o;\
e.o-cbuf.o|ds.o|print.o|mh.o|l.o|st.o|va.o;\
fd.o-cbuf.o|print.o|e.o|net.o|l.o|ds.o|echo.o|mh.o|va.o;\
conn.o-cbuf.o|fd.o|print.o|mh.o|ds.o|va.o;\
echo.o-cbuf.o|mh.o|print.o|ds.o|e.o|va.o;\
stat.o-cbuf.o|te.o|ds.o|l.o|print.o|e.o|va.o;\
st.o-print.o;\
ip.o-cbuf.o|if.o|va.o;\
port.o-cbuf.o|l.o;\
cm.o-cbuf.o|print.o|mh.o|echo.o|[alt_]echo.o|ds.o|va.o;\
if.o-cbuf.o|print.o|mh.o|l.o|ds.o|va.o;\
schedconf.o-print.o;\
vm.o-ds.o|print.o|mm.o|l.o|boot.o;\
va.o-ds.o|print.o|mm.o|l.o|boot.o|vm.o;\
boot.o-print.o|ds.o|mm.o|cg.o;\
\
mpd.o-cbuf.o|cg.o|ds.o|print.o|te.o|mh.o|va.o;\
cg.o-ds.o;\
bc.o-print.o\
" ./gen_client_stub


