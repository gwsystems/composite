#!/bin/sh

# httperf --server=10.0.2.8 --port=200 --uri=/fs/bar --num-conns=7000
./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!sm.o,a1;!mpool.o, ;!buf.o, ;!bufp.o, ;!va.o, a3;!tif.o,a5;!tip.o, ;\
!port.o, ;!l.o,a4;!te.o,a3;!tnet.o, ;!eg.o,a5;\
!stconnmt.o, '10:10:200:/bind:0:%d/listen:255';\
!pfs.o, ;!httpt.o,a8;!rotar.o,a7;!initfs.o,a3:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
tnet.o-sm.o|fprr.o|mm.o|print.o|l.o|te.o|eg.o|[parent_]tip.o|port.o|va.o|buf.o|bufp.o|pfs.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o|pfs.o;\
mm.o-[parent_]llboot.o|print.o;\
eg.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o|pfs.o;\
stconnmt.o-sm.o|print.o|fprr.o|mm.o|va.o|l.o|httpt.o|[from_]tnet.o|buf.o|bufp.o|eg.o|pfs.o;\
httpt.o-sm.o|l.o|print.o|fprr.o|mm.o|buf.o|bufp.o|[server_]rotar.o|te.o|va.o|pfs.o;\
rotar.o-sm.o|fprr.o|print.o|mm.o|buf.o|bufp.o|l.o|eg.o|va.o|initfs.o|pfs.o;\
initfs.o-fprr.o|print.o|buf.o|bufp.o|va.o|l.o|mm.o;\
tip.o-sm.o|[parent_]tif.o|va.o|fprr.o|print.o|l.o|eg.o|buf.o|bufp.o|mm.o|pfs.o;\
port.o-sm.o|l.o|print.o|pfs.o;\
tif.o-sm.o|print.o|fprr.o|mm.o|l.o|va.o|eg.o|buf.o|bufp.o|pfs.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
pfs.o-fprr.o|sm.o|mm.o|print.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|pfs.o;\
bufp.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|buf.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o\
" ./gen_client_stub
