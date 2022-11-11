#!/bin/sh

cp unit_fpu_test.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
