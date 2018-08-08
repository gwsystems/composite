#!/bin/sh

cp root_fprr.o boot.o
cp llboot_comp.o llboot.o
./cos_linker "llboot.o, ;capmgr.o, ;*rumpcos.o,'rH';http.o, ;*boot.o, ;udpserv.o, :boot.o-capmgr.o;rumpcos.o-capmgr.o|[parent_]boot.o;http.o-rumpcos.o|capmgr.o;udpserv.o-capmgr.o|rumpcos.o" ./gen_client_stub
