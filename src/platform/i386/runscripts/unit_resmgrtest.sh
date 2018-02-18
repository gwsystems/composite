#!/bin/sh

cp llboot_comp.o llboot.o
cp unitresmgr.o 2_0_unitresmgr.o
./cos_linker "llboot.o, ;resmgr.o, ;2_0_unitresmgr.o, :2_0_unitresmgr.o-resmgr.o" ./gen_client_stub
