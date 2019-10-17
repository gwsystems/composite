#!/bin/sh

cp llboot_comp.o llboot.o
cp test_sched.o boot.o
cp test_sched_inv.o intcomp.o
cp test_sched_inv.o w1comp.o
cp test_sched_inv.o w3comp.o
cp test_boot.o dummy1.o
cp test_boot.o dummy2.o

# only int and w0 in root
./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;dummy2.o, ;*boot.o, ;intcomp.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o" ./gen_client_stub

#int, w0 in root and w1 in comp
#./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;dummy2.o, ;*boot.o, ;intcomp.o, ;w1comp.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o;w1comp.o-boot.o|capmgr.o" ./gen_client_stub

# int, w1 - w3
#./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;dummy2.o, ;*boot.o, ;intcomp.o, ;w1comp.o, ;w3comp.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o;w1comp.o-boot.o|capmgr.o;w3comp.o-boot.o|capmgr.o" ./gen_client_stub

#cp test_boot.o dummy.o
#./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;dummy2.o, ;*boot.o, ;intcomp.o, :boot.o-capmgr.o;intcomp.o-boot.o|capmgr.o" ./gen_client_stub
#
