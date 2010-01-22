#!/bin/sh

./cos_loader \
"c0.o, ;*fprr.o, ;mpd.o,a4;l.o,a8;mm.o, ;print.o, ;te.o,a3;net.o,a6;e.o,a3;fd.o,a8;conn.o,a9;http.o,a8;\
stat.o,a25;st.o, ;cm.o,a7;sc.o,a6;if.o,a5;ip.o, ;ainv.o,a6;fn.o, ;cgi.o,a9;port.o, ;schedconf.o, ;\
bc.o, ;(fd2.o=fd.o),a8;(fd3.o=fd.o),a8;(cgi2.o=cgi.o),a9;(ainv2.o=ainv.o),a6;\
(*fprrc1.o=fprr.o),a11;(*fprrc2.o=fprr.o),a12;(*fprrc3.o=fprr.o),a11:\
\
net.o-fprr.o|mm.o|print.o|l.o|te.o|e.o|ip.o|port.o;\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|st.o|schedconf.o|[parent_]bc.o;\
fprrc1.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
fprrc2.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprr.o;\
fprrc3.o-print.o|mm.o|st.o|schedconf.o|[parent_]fprrc2.o;\
mpd.o-fprr.o|print.o|te.o|mm.o;\
l.o-fprr.o|mm.o|print.o|te.o;\
te.o-print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-fprr.o|print.o|mm.o|l.o|st.o;\
fd.o-print.o|e.o|net.o|l.o|fprr.o|http.o|mm.o;\
conn.o-fd.o|print.o|mm.o|fprrc1.o;\
http.o-mm.o|print.o|fprr.o|cm.o|te.o;\
stat.o-te.o|fprr.o|l.o|print.o|e.o;\
st.o-print.o;\
ip.o-if.o;\
port.o-l.o;\
cm.o-print.o|mm.o|sc.o|fprr.o|ainv.o|[alt_]ainv2.o;\
sc.o-print.o|mm.o|e.o|fprr.o;\
if.o-print.o|mm.o|l.o|fprr.o;\
fn.o-fprr.o;\
fd2.o-fn.o|ainv.o|print.o|mm.o|fprr.o|e.o|l.o;\
ainv.o-mm.o|print.o|fprr.o|l.o|e.o;\
cgi.o-fd2.o|fprrc1.o|print.o;\
fd3.o-fn.o|ainv2.o|print.o|mm.o|fprr.o|e.o|l.o;\
ainv2.o-mm.o|print.o|fprr.o|l.o|e.o;\
cgi2.o-fd3.o|fprrc1.o|print.o;\
schedconf.o-print.o;\
bc.o-print.o\
" ./gen_client_stub
