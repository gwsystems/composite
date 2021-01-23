#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
cp hier_fprr.o sched_client.o
./cos_linker "llboot.o, ;*sched_client.o, ;capmgr.o, ;*srvdummy_server.o, ;*boot.o, ;srvdummy_stubcomp.o, ;unitsrvdummyacom.o, :boot.o-capmgr.o;sched_client.o-capmgr.o|[parent_]boot.o;srvdummy_server.o-capmgr.o|[parent_]boot.o;unitsrvdummyacom.o-capmgr.o|sched_client.o;srvdummy_stubcomp.o-capmgr.o|srvdummy_server.o" ./gen_client_stub
