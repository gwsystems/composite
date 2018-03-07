#!/bin/sh

cp network_interface_nf.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
