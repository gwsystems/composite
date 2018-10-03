#!/bin/sh

cp llboot_comp.o llboot.o
cp micro_root.o boot.o
cp test_boot.o dummy1.o
./cos_linker "llboot.o, ;capmgr.o, ;*microvm.o, ;dummy1.o, ;*boot.o, :boot.o-capmgr.o;microvm.o-capmgr.o|boot.o" ./gen_client_stub
