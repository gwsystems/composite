#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;shmem.o, ;robot_controller.o, ;robot_scheduler.o, :robot_controller.o-shmem.o;robot_scheduler.o-robot_controller.o" ./gen_client_stub
