#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;resmgr.o, ;root_fprr_sched.o, ;_2_unit_schedcomp_test.o, :root_fprr_sched.o-resmgr.o;_2_unit_schedcomp_test.o-root_fprr_sched.o" ./gen_client_stub
