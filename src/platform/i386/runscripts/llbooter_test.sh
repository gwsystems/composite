#!/bin/sh

cp llboot_test.o llboot.o
cp test_boot.o sl_test_boot.o
./cos_linker "llboot.o, ;sl_test_boot.o, :" ./gen_client_stub
