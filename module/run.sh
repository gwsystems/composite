#!/bin/sh

./cos_loader \
"c0.o,fprr.o,wftest.o,mpd.o,l.o,mm.o,print.o,te.o,net.o,e.o,fd.o,conn.o,http.o:\
net.o-fprr.o;net.o-mm.o;c0.o-fprr.o;fprr.o-print.o;mpd.o-fprr.o;mpd.o-print.o;\
wftest.o-l.o;wftest.o-fprr.o;wftest.o-te.o;l.o-fprr.o;l.o-mm.o;l.o-print.o;\
l.o-te.o;te.o-print.o;te.o-fprr.o;mm.o-print.o;wftest.o-print.o;net.o-print.o;\
net.o-l.o;mm.o-print.o;net.o-te.o;net.o-e.o;e.o-fprr.o;e.o-print.o;e.o-mm.o;\
e.o-l.o;fd.o-print.o;fd.o-e.o;fd.o-net.o;fd.o-l.o;fd.o-fprr.o;conn.o-fd.o;\
conn.o-print.o;conn.o-mm.o;conn.o-fprr.o;fd.o-http.o;http.o-mm.o;http.o-print.o;\
http.o-fprr.o;http.o-e.o;fd.o-mm.o;fprr.o-mm.o" \
./gen_client_stub
