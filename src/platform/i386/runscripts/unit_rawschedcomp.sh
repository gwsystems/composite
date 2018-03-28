#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr_raw.o boot.o
cp unit_schedcomp_test.o test_sched1.o
cp unit_schedcomp_test.o test_sched2.o
cp unit_schedcomp_test.o test_sched3.o
./cos_linker "llboot.o, ;test_sched1.o, ;test_sched2.o, ;test_sched3.o, ;*boot.o, :test_sched1.o-boot.o;test_sched2.o-boot.o;test_sched3.o-boot.o" ./gen_client_stub
