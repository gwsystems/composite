#!/bin/sh

cp llboot_comp.o llboot.o
./cos_linker "llboot.o, ;test_boot.o, :" ./gen_client_stub
