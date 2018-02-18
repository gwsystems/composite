#!/bin/sh

cp llboot_comp.o llboot.o
cp ppong.o _0_ppong.o
cp ppong_two.o _0_ppong_two.o
cp pingp.o _0_pingp.o
cp pingp_two.o _0_pingp_two.o
./cos_linker "llboot.o, ;_0_ppong.o, ;_0_ppong_two.o, ;_0_pingp.o, ;_0_pingp_two.o, :_0_pingp.o-_0_ppong.o;_0_pingp_two.o-_0_ppong_two.o" ./gen_client_stub
