#!/bin/sh

cp rust_test.o llboot.o
./cos_linker "llboot.o, ;_0_test_boot.o, :" ./gen_client_stub
