#!/bin/sh

cp no_interface.scheddev.o llboot.o
./cos_linker "llboot.o, ;llpong.o, :" ./gen_client_stub
