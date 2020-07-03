#!/bin/sh

# hierarchical wakeup latency

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o,a2;schedconf.o, ;cg.o,a1;bc.o, ;st.o, ;\
\
!mpd.o,a5;\
\
(*fprrc2.o=fprr.o),a4;(*fprrc3.o=fprr.o),a4;(*fprrc4.o=fprr.o),a4;\
\
!wkup.o,a6;!te.o,a3:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
fprrc2.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
fprrc3.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc2.o;\
fprrc4.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc3.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
cg.o-fprr.o;\
mm.o-print.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
\
te.o-cbuf.o|print.o|fprr.o|mm.o;\
mpd.o-cbuf.o|fprr.o|print.o|te.o|mm.o|cg.o;\
\
wkup.o-cbuf.o|print.o|fprr.o\
" ./gen_client_stub
