#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
cp test_boot.o dummy.o
./cos_linker "llboot.o, ;capmgr.o, ;micro_schedovhdtest.o, ;dummy.o, ;*boot.o, :boot.o-capmgr.o;micro_schedovhdtest.o-capmgr.o|boot.o" ./gen_client_stub
