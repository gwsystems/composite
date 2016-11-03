#!/bin/sh

cp cFE_booter.o llboot.o
./cos_linker "llboot.o, ;llpong.o, :" ./gen_client_stub
