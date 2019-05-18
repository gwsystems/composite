#!/bin/sh

cp unit_slxcoretests.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
