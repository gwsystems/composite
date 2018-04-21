#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
./cos_linker "llboot.o, ;unit_schedcomp_test.o,'cpu=00,';capmgr.o, ;unit_schedaep_test.o,'cpu=11,';*boot.o, :boot.o-capmgr.o;unit_schedcomp_test.o-boot.o;unit_schedaep_test.o-boot.o|capmgr.o" ./gen_client_stub
