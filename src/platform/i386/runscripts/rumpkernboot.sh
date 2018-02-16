#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;shmem.o, ;sl_rumpcos.o, ;udpserv.o, :sl_rumpcos.o-shmem.o;udpserv.o-sl_rumpcos.o;udpserv.o-shmem.o" ./gen_client_stub -v
