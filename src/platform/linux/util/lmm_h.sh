#!/bin/sh

# Basic configuration to startup a limited web-server running on port 200 serving /hw, /cgi/hw, and /cgi/HW.
# Can be tested with: httperf --server=10.0.2.8 --port=200 --uri=/cgi/hw --num-conns=7000

./cos_loader \
"c0.o, ;*ds.o, ;mm.o, ;mh.o, ;(mh2.o=mh.o), ;(mh3.o=mh.o), ;(mh4.o=mh.o), ;mht.o,a2;print.o, ;schedconf.o, ;cg.o,a1;bc.o, ;st.o, :\
\
c0.o-ds.o;\
ds.o-print.o|mh.o|st.o|schedconf.o|[parent_]bc.o;\
mm.o-print.o;\
mh.o-[parent_]mm.o|[main_]mm.o|print.o;\
mh2.o-[parent_]mh.o|[main_]mm.o|print.o;\
mh3.o-[parent_]mh2.o|[main_]mm.o|print.o;\
mh4.o-[parent_]mh3.o|[main_]mm.o|print.o;\
mht.o-print.o|ds.o|mh4.o;\
st.o-print.o;\
schedconf.o-print.o;\
cg.o-ds.o;\
bc.o-print.o\
" ./gen_client_stub
