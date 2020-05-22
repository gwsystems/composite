#!/bin/sh

cp kernel_tests.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
