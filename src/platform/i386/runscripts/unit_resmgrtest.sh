#!/bin/sh

cp llboot_comp.o llboot.o
cp test_boot.o   dummy1.o
cp resmgr.o      mm.o
./cos_linker "llboot.o, ;dummy1.o, ;mm.o, ;*unitresmgr.o, ;*unitresmgr_shmmap.o, :unitresmgr.o-mm.o;unitresmgr_shmmap.o-mm.o" ./gen_client_stub
