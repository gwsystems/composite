#!/bin/sh

cp llboot_comp.o llboot.o
cp microipi.o boot.o
cp test_boot.o dummy1.o
cp test_boot.o dummy2.o
./cos_linker "llboot.o, ;capmgr.o, ;dummy1.o, ;dummy2.o, ;*boot.o, :boot.o-capmgr.o" ./gen_client_stub
