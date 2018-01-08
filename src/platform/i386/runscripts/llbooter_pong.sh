#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;robot_controller.o, :" ./gen_client_stub
