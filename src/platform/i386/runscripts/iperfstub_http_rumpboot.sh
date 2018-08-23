#!/bin/sh

cp root_fprr.o boot.o
cp llboot_comp.o llboot.o
cp rk_stubcomp.o iperfstub.o
./cos_linker "llboot.o, ;capmgr.o, ;*rumpcos.o,'rH,c01';iperf.o, ;*boot.o, ;http.o, ;iperfstub.o,'r1';*rk_dummy.o,'c10,r1':boot.o-capmgr.o;rumpcos.o-capmgr.o|[parent_]boot.o;rk_dummy.o-capmgr.o|[parent_]boot.o;iperf.o-capmgr.o|rk_dummy.o;iperfstub.o-capmgr.o|rumpcos.o;http.o-capmgr.o|rumpcos.o" ./gen_client_stub
