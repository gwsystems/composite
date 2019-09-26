#!/bin/sh

cp llboot_comp.o llboot.o
cp test_sched.o boot.o
cp test_sched_inv.o intcomp.o
cp test_sched_inv.o w1comp.o
cp test_boot.o dummy1.o
cp test_boot.o dummy2.o
#./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;dummy2.o, ;*boot.o, ;intcomp.o, ;w1comp.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o;w1comp.o-boot.o|capmgr.o" ./gen_client_stub
#./cos_linker "llboot.o, ;intcomp.o, ;capmgr.o, ;w1comp.o, ;*boot.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o;w1comp.o-boot.o|capmgr.o" ./gen_client_stub
cp test_sched_inv.o w3comp.o
./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;dummy2.o, ;*boot.o, ;intcomp.o, ;w1comp.o, ;w3comp.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o;w1comp.o-boot.o|capmgr.o;w3comp.o-boot.o|capmgr.o" ./gen_client_stub

#cp test_boot.o dummy.o
#./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;dummy2.o, ;*boot.o, ;intcomp.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o" ./gen_client_stub
#
