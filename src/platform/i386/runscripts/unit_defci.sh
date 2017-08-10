#!/bin/sh

cp unit_defci.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
