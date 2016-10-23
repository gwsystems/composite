#!/bin/sh

cp cFE_boot.o llboot.o
./cos_linker "llboot.o, ;llpong.o, :" ./gen_client_stub
