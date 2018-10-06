#!/bin/sh

cp micro_sltests.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
