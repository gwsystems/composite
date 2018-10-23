#!/bin/sh

cp llboot_comp.o llboot.o
./cos_linker "llboot.o, ;ppong.o, ;pingp.o, :pingp.o-ppong.o" ./gen_client_stub
