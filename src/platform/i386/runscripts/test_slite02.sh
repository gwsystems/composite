#!/bin/sh

cp llboot_comp.o llboot.o
cp test_sched.o boot.o
cp test_sched_inv.o intcomp.o
#cp test_sched_inv.o w1comp.o
#cp test_sched_inv.o w3comp.o
#./cos_linker "llboot.o, ;intcomp.o, ;capmgr.o, ;w1comp.o, ;*boot.o, ;w3comp.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o;w1comp.o-boot.o|capmgr.o;w3comp.o-boot.o|capmgr.o" ./gen_client_stub

cp test_boot.o dummy.o
./cos_linker "llboot.o, ;intcomp.o, ;capmgr.o, ;dummy.o, ;*boot.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o" ./gen_client_stub
