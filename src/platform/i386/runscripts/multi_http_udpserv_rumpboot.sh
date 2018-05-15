#!/bin/sh

cp root_fprr.o boot.o
cp llboot_comp.o llboot.o
./cos_linker "llboot.o, ;capmgr.o, ;*rumpcos.o, ;http.o, ;*boot.o, ;udpserv.o, ;*rk_dummy.o,'r1,';rk_stubcomp.o,'r1,':boot.o-capmgr.o;rumpcos.o-capmgr.o|[parent_]boot.o;http.o-rumpcos.o|capmgr.o;rk_dummy.o-capmgr.o|[parent_]boot.o;udpserv.o-capmgr.o|rk_dummy.o;rk_stubcomp.o-capmgr.o|rumpcos.o" ./gen_client_stub
