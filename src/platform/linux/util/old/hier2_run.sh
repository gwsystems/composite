#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mpd.o,a4;l.o,a8;mm.o, ;print.o, ;te.o,a3;e.o,a3;schedconf.o, ;stat.o,a25;st.o, ;bc.o, ;\
(*fprrc2.o=fprr.o),a4;(*fprrc3.o=fprr.o),a4;(*fprrc4.o=fprr.o),a4;\
p.o,a4;(p1.o=p.o),a5:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
mpd.o-fprr.o|print.o|te.o|mm.o;\
l.o-fprr.o|mm.o|print.o|te.o;\
te.o-print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o;\
stat.o-te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
fprrc2.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
fprrc3.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc2.o;\
fprrc4.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc3.o;\
p.o-print.o|te.o|fprrc4.o|e.o;\
p1.o-print.o|te.o|fprrc4.o|e.o\
" ./gen_client_stub
