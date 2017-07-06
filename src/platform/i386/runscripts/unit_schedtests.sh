#!/bin/sh

cp no_interface.scheddev.o llboot.o
./cos_linker "llboot.o, ;test_boot.o, :" ./gen_client_stub
