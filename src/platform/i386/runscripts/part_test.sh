#!/bin/sh

cp part_test.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
