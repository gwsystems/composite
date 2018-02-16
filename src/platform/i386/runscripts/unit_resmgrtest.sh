#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;resmgr.o, ;_0_unitresmgr.o, :_0_unitresmgr.o-resmgr.o" ./gen_client_stub
