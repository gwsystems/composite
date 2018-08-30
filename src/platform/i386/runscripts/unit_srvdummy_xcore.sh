#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
# srvdummy_client and unit_srvdummy run on core 0
# srvdummy_server and srvdummy_stubcomp run on core 1
# this tests cross core SINV => async requests
# to test this, set NUM_CPU to 2 in cos_config.h or remove 'cpu=xx,' args in the below line.
./cos_linker "llboot.o, ;*srvdummy_client.o,'c01';capmgr.o, ;*srvdummy_server.o,'c10';*boot.o, ;srvdummy_stubcomp.o, ;unitsrvdummy.o, :boot.o-capmgr.o;srvdummy_client.o-capmgr.o|[parent_]boot.o;srvdummy_server.o-capmgr.o|[parent_]boot.o;unitsrvdummy.o-srvdummy_client.o;srvdummy_stubcomp.o-capmgr.o|srvdummy_server.o" ./gen_client_stub
