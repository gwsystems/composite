#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: 
# httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;boot.o, ;print.o, ;\
\
!tasc.o, ;(!stconn2.o=stconn.o), ;!mpool.o, ;!cbuf.o, ;!va.o, ;!vm.o, ;\
!mpd.o,a5;!tif.o,a5;!tip.o, ;!port.o, ;!l.o,a4;!te.o,a3;!tnet.o, ;\
!eg.o,a5;!stconn.o,a9;!httpt.o,a8;!rotar.o,a7;!initfs.o,a3:\
\
c0.o-fprr.o;\
fprr.o-print.o|[parent_]mm.o;\
tnet.o-fprr.o|mm.o|print.o|l.o|te.o|eg.o|[parent_]tip.o|port.o|va.o|cbuf.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o|va.o;\
mm.o-print.o;\
eg.o-cbuf.o|fprr.o|print.o|mm.o|l.o|va.o;\
stconn.o-print.o|mm.o|fprr.o|va.o|l.o|httpt.o|[from_]tnet.o|cbuf.o|eg.o;\
httpt.o-l.o|print.o|fprr.o|mm.o|cbuf.o|[server_]tasc.o|te.o|va.o;\
stconn2.o-print.o|mm.o|fprr.o|va.o|l.o|rotar.o|[from_]tasc.o|cbuf.o|eg.o;\
tasc.o-fprr.o|print.o|mm.o|cbuf.o|l.o|eg.o|va.o;\
rotar.o-fprr.o|print.o|mm.o|cbuf.o|l.o|eg.o|va.o|initfs.o;\
initfs.o-fprr.o|print.o;\
tip.o-[parent_]tif.o|va.o|fprr.o|print.o|l.o|eg.o|cbuf.o|mm.o;\
port.o-cbuf.o|l.o;\
tif.o-print.o|fprr.o|mm.o|l.o|va.o|eg.o|cbuf.o;\
boot.o-print.o|fprr.o|mm.o;\
\
\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
cbuf.o-boot.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
mpd.o-cbuf.o|boot.o|fprr.o|print.o|te.o|mm.o|va.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o\
" ./gen_client_stub


