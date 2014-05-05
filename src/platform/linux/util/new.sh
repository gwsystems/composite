#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, :\
c0.o-llboot.o\
" ./gen_client_stub

#;llping.o, ;llpong.o, 
#llping.o-llpong.o;\



