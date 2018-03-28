#!/bin/sh

cp llboot_comp.o llboot.o
cp ppong.o ppong_two.o
cp pingp.o pingp_two.o
./cos_linker "llboot.o, ;ppong.o, ;ppong_two.o, ;pingp.o, ;pingp_two.o, :pingp.o-ppong.o;pingp_two.o-ppong_two.o" ./gen_client_stub
