#!/bin/sh

cp llboot_comp.o llboot.o
cp test_boot.o dummy1.o
cp test_boot.o dummy2.o
cp resmgr.o mm.o
cp fprr_sched.o boot.o
./cos_linker "llboot.o, ;dummy1.o, ;mm.o, ;dummy2.o, ;*boot.o, ;unit_schedcomp_test.o, ;unit_schedaep_test.o, :boot.o-mm.o;unit_schedcomp_test.o-boot.o;unit_schedaep_test.o-boot.o|mm.o" ./gen_client_stub
