#!/bin/sh

cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;shmem.o, ;camera_cont.o, ;robot_controller.o, ;robot_scheduler.o, ;car_mgr1.o, :robot_controller.o-shmem.o;camera_cont.o-shmem.o;robot_controller.o-camera_cont.o;robot_scheduler.o-robot_controller.o;robot_scheduler.o-shmem.o;car_mgr1.o-robot_scheduler.o" ./gen_client_stub
