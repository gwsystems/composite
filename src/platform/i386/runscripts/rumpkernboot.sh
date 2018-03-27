#!/bin/sh

cp root_fprr.o boot.o
cp llboot_comp.o llboot.o
./cos_linker "llboot.o, ;capmgr.o, ;*rumpcos.o, ;udpserv.o, ;*boot.o, :boot.o-capmgr.o;rumpcos.o-capmgr.o|[parent_]boot.o;udpserv.o-rumpcos.o;udpserv.o-capmgr.o" ./gen_client_stub
