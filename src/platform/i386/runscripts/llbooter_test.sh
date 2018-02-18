#!/bin/sh

cp llboot_comp.o llboot.o
cp test_boot.o _0_test_boot.o
./cos_linker "llboot.o, ;_0_test_boot.o, :" ./gen_client_stub
