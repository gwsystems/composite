#!/bin/sh

cp micro_xcore.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
