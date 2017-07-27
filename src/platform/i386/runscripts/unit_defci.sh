#!/bin/sh

cp unit_defci.o llboot.o
./cos_linker "llboot.o, ;test_boot.o, :" ./gen_client_stub
