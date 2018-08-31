#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
./cos_linker "llboot.o, ;par_test_comp.o, ;capmgr.o, ;test_boot.o, ;*boot.o, :boot.o-capmgr.o;par_test_comp.o-boot.o" ./gen_client_stub
