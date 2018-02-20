#!/bin/sh

cp llboot_comp.o llboot.o
cp unitresmgr.o 2_0_unitresmgr.o
cp unitresmgr_two.o _0_unitresmgr_two.o
./cos_linker "llboot.o, ;resmgr.o, ;2_0_unitresmgr.o, ;_0_unitresmgr_two.o, :2_0_unitresmgr.o-resmgr.o;_0_unitresmgr_two.o-resmgr.o" ./gen_client_stub
