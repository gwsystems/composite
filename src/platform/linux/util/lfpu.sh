#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!fpu.o,a1;(!fpu1.o=fpu.o),a1:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
fpu.o-print.o|fprr.o|mm.o;\
fpu1.o-print.o|fprr.o|mm.o\
" ./gen_client_stub
