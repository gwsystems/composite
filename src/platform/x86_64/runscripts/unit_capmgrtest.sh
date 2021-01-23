#!/bin/sh

cp llboot_comp.o llboot.o
cp test_boot.o   dummy1.o
./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;*unitcapmgr.o, ;*unitcapmgr_shmmap.o, :unitcapmgr.o-capmgr.o;unitcapmgr_shmmap.o-capmgr.o" ./gen_client_stub
