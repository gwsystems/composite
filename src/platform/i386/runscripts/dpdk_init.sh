#!/bin/sh

cp cos_dpdk.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
