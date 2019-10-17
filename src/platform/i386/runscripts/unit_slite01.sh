#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
#cp unit_slrcvtest.o boot.o
#cp test_boot.o dummy1.o
#cp test_boot.o dummy2.o
./cos_linker "llboot.o, ;*spin_comp.o, ;capmgr.o, ;*unit_slrcvtest.o, ;*boot.o, :boot.o-capmgr.o;unit_slrcvtest.o-boot.o|capmgr.o;spin_comp.o-boot.o|capmgr.o" ./gen_client_stub
#./cos_linker "llboot.o, ;dummy2.o, ;capmgr.o, ;dummy1.o, ;*boot.o, :boot.o-capmgr.o" ./gen_client_stub
