#!/bin/sh

cp noiso_rumpcos.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub -v
