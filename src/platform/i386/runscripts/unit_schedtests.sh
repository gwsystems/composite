#!/bin/sh

cp unit_schedlibtests.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
