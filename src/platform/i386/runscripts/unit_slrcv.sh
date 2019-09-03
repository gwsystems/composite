#!/bin/sh

cp unit_slrcvtest.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
