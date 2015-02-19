#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o,a2;schedconf.o, ;cg.o,a1;bc.o, ;st.o, ;\
\
!mpd.o,a5;\
\
(*fprrc2.o=fprr.o),a6;(*fprrc3.o=fprr.o),a4;(*fprrc4.o=fprr.o),a4;\
\
!e.o,a5;!te.o,a3;!l.o,a4;!p.o,a8;(!p1.o=p.o),a9:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
fprrc2.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
fprrc3.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc2.o;\
fprrc4.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc3.o;\
mm.o-print.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
cg.o-fprr.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
\
mpd.o-cbuf.o|fprr.o|print.o|te.o|mm.o|cg.o;\
\
l.o-cbuf.o|fprr.o|mm.o|print.o|te.o;\
te.o-cbuf.o|print.o|fprr.o|mm.o;\
e.o-cbuf.o|fprr.o|print.o|mm.o|l.o|st.o;\
p.o-cbuf.o|print.o|te.o|fprrc4.o|e.o;\
p1.o-cbuf.o|print.o|te.o|fprr.o|e.o\
" ./gen_client_stub


