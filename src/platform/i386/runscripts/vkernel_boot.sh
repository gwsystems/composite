#!/bin/sh

cp vkernel_boot.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
