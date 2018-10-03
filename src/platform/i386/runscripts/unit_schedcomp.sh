#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
./cos_linker "llboot.o, ;unit_schedcomp_test.o, ;capmgr.o, ;test_boot.o, ;*boot.o, :boot.o-capmgr.o;unit_schedcomp_test.o-boot.o|capmgr.o" ./gen_client_stub
#./cos_linker "llboot.o, ;unit_schedcomp_test.o,'c0,';capmgr.o, ;unit_schedaep_test.o,'c1,';*boot.o, :boot.o-capmgr.o;unit_schedcomp_test.o-boot.o;unit_schedaep_test.o-boot.o|capmgr.o" ./gen_client_stub
