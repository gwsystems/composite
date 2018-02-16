#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;_0_ppong.o, ;_0_ppong_two.o, ;_0_pingp.o, ;_0_pingp_two.o, :_0_pingp.o-_0_ppong.o;_0_pingp_two.o-_0_ppong_two.o" ./gen_client_stub
