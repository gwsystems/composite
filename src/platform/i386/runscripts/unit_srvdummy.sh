#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
./cos_linker "llboot.o, ;*srvdummy_client.o, ;capmgr.o, ;*srvdummy_server.o, ;*boot.o, ;srvdummy_stubcomp.o, ;unitsrvdummy.o, :boot.o-capmgr.o;srvdummy_client.o-capmgr.o|[parent_]boot.o;srvdummy_server.o-capmgr.o|[parent_]boot.o;unitsrvdummy.o-srvdummy_client.o;srvdummy_stubcomp.o-capmgr.o|srvdummy_server.o" ./gen_client_stub
