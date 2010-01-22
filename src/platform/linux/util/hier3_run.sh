#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;st.o, ;bc.o, ;\
(*fprrc2.o=fprr.o),a4;(*fprrc3.o=fprr.o),a4;(*fprrc4.o=fprr.o),a4;\
wkup.o,a3:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
mm.o-print.o;\
st.o-print.o;\
schedconf.o-print.o;\
bc.o-print.o;\
fprrc2.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
fprrc3.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc2.o;\
fprrc4.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc3.o;\
wkup.o-print.o|fprrc3.o\
" ./gen_client_stub
