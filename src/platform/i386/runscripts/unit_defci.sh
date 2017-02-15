#!/bin/sh

cp unit_defci.o llboot.o
./cos_linker "llboot.o, ;llpong.o, :" ./gen_client_stub
