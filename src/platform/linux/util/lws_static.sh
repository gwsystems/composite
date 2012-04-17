#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: 
# httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;boot.o, ;print.o, ;\
\
!sm.o,a1;!mpool.o, ;!buf.o, ;!va.o, ;!mpd.o,a5;!cm.o,a7;!sc.o,a6;!map.o,a6;!if.o,a5;!ip.o, ;\
!port.o, ;!l.o,a4;!te.o,a3;!net.o, ;!e.o,a5;!stconn.o,a9;!pfs.o, ;\
!httpt.o,a8;!rotar.o,a7;!initfs.o,a3:\
\
c0.o-fprr.o;\
fprr.o-print.o|[parent_]mm.o;\
net.o-sm.o|fprr.o|mm.o|print.o|l.o|te.o|e.o|ip.o|port.o|va.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o;\
stconn.o-sm.o|print.o|mm.o|fprr.o|va.o|l.o|httpt.o|net.o|buf.o|e.o|pfs.o;\
httpt.o-sm.o|l.o|print.o|fprr.o|mm.o|buf.o|[server_]rotar.o|te.o|va.o|pfs.o;\
rotar.o-sm.o|fprr.o|print.o|mm.o|buf.o|l.o|e.o|va.o|initfs.o|pfs.o;\
initfs.o-fprr.o|print.o;\
ip.o-sm.o|if.o|va.o;\
port.o-sm.o|l.o;\
cm.o-sm.o|print.o|mm.o|sc.o|[alt_]map.o|fprr.o|va.o;\
sc.o-sm.o|print.o|mm.o|e.o|fprr.o|va.o;\
map.o-sm.o|print.o|mm.o|e.o|fprr.o|va.o;\
if.o-sm.o|print.o|fprr.o|mm.o|l.o|va.o;\
boot.o-print.o|fprr.o|mm.o;\
pfs.o-sm.o|mm.o|print.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
mpd.o-sm.o|boot.o|fprr.o|print.o|te.o|mm.o|va.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o\
" ./gen_client_stub


