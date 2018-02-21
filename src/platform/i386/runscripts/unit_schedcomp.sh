#!/bin/sh

cp llboot_comp.o llboot.o
cp fprr_sched.o root_fprr_sched.o
cp unit_schedcomp_test.o _2_unit_schedcomp_test.o
cp unit_schedaep_test.o _2_unit_schedaep_test.o
./cos_linker "llboot.o, ;resmgr.o, ;root_fprr_sched.o, ;_2_unit_schedcomp_test.o, ;_2_unit_schedaep_test.o, :root_fprr_sched.o-resmgr.o;_2_unit_schedcomp_test.o-root_fprr_sched.o;_2_unit_schedaep_test.o-root_fprr_sched.o|resmgr.o" ./gen_client_stub
