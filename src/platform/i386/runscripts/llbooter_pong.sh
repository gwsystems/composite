#!/bin/sh

cp llboot_test.o llboot.o
cp ppong.o       sl_ppong.o
cp ppong_two.o   sl_ppong_two.o
cp pingp.o       sl_pingp.o
cp pingp_two.o   sl_pingp_two.o
./cos_linker "llboot.o, ;sl_ppong.o, ;sl_ppong_two.o, ;sl_pingp.o, ;sl_pingp_two.o, :sl_pingp.o-sl_ppong.o;sl_pingp_two.o-sl_ppong_two.o" ./gen_client_stub
