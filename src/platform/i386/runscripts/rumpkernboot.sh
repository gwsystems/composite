#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;shmem.o, ;rumpcos.o, ;udpserv.o, :rumpcos.o-shmem.o;udpserv.o-rumpcos.o" ./gen_client_stub -v
