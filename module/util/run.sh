#!/bin/sh

./cos_loader \
"c0.o,fprr.o,wftest.o,mpd.o,l.o,mm.o,print.o,te.o,net.o,e.o,fd.o,conn.o,http.o,\
stat.o,st.o,cm.o,sc.o,if.o,ip.o,ainv.o,fn.o,fd2.o,cgi.o,fd3.o,cgi2.o:\
\
net.o-fprr.o;net.o-mm.o;net.o-print.o;net.o-l.o;net.o-te.o;net.o-e.o;net.o-ip.o;\
c0.o-fprr.o;\
fprr.o-print.o;fprr.o-mm.o;fprr.o-st.o;\
mpd.o-fprr.o;mpd.o-print.o;mpd.o-te.o;mpd.o-mm.o;\
wftest.o-l.o;wftest.o-fprr.o;wftest.o-te.o;wftest.o-print.o;\
l.o-fprr.o;l.o-mm.o;l.o-print.o;l.o-te.o;\
te.o-print.o;te.o-fprr.o;te.o-mm.o;\
mm.o-print.o;mm.o-print.o;\
e.o-fprr.o;e.o-print.o;e.o-mm.o;e.o-l.o;e.o-st.o;\
fd.o-print.o;fd.o-e.o;fd.o-net.o;fd.o-l.o;fd.o-fprr.o;fd.o-http.o;fd.o-mm.o;\
conn.o-fd.o;conn.o-print.o;conn.o-mm.o;conn.o-fprr.o;\
http.o-mm.o;http.o-print.o;http.o-fprr.o;http.o-e.o;http.o-cm.o;\
stat.o-te.o;stat.o-fprr.o;stat.o-l.o;stat.o-print.o;stat.o-e.o;\
st.o-print.o;\
ip.o-if.o;\
cm.o-print.o;cm.o-mm.o;cm.o-sc.o;cm.o-fprr.o;cm.o-ainv.o;\
sc.o-print.o;sc.o-mm.o;sc.o-e.o;sc.o-fprr.o;\
if.o-print.o;if.o-mm.o;if.o-l.o;if.o-fprr.o;\
fd2.o-fn.o;fd2.o-ainv.o;fd2.o-print.o;fd2.o-mm.o;fd2.o-fprr.o;fd2.o-e.o;fd2.o-l.o;\
ainv.o-mm.o;ainv.o-print.o;ainv.o-fprr.o;ainv.o-l.o;ainv.o-e.o;\
cgi.o-fd2.o;cgi.o-fprr.o;cgi.o-print.o;\
fd3.o-fn.o;fd3.o-ainv.o;fd3.o-print.o;fd3.o-mm.o;fd3.o-fprr.o;fd3.o-e.o;fd3.o-l.o;\
cgi2.o-fd3.o;cgi2.o-fprr.o;cgi2.o-print.o\
" ./gen_client_stub
