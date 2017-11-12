#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;ppong.o, ;pingp.o, ;pingp_two.o, :pingp.o-ppong.o;pingp_two.o-ppong.o" ./gen_client_stub
