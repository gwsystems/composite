#!/bin/sh

# FIXME HACK used to test tlsmgr component without having a dedicated scheduler component
cp tlsmgr.o sl_tlsmgr.o

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;sl_tlsmgr.o, ;shmem.o, ;sl_rumpcos.o, ;udpserv.o, :sl_rumpcos.o-shmem.o;udpserv.o-sl_rumpcos.o;udpserv.o-shmem.o;sl_rumpcos.o-sl_tlsmgr.o" ./gen_client_stub -v
