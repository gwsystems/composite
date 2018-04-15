#!/bin/sh

cp root_fprr.o boot.o
cp llboot_comp.o llboot.o
./cos_linker "llboot.o, ;capmgr.o, ;*rumpcos.o, ;udpserv.o, ;*boot.o, ;car_mgr1.o, ;robot_controller.o, ;camera_cont.o, :boot.o-capmgr.o;rumpcos.o-capmgr.o|[parent_]boot.o|camera_cont.o;udpserv.o-rumpcos.o;udpserv.o-capmgr.o|camera_cont.o;car_mgr1.o-capmgr.o|boot.o|robot_controller.o;robot_controller.o-capmgr.o|boot.o|udpserv.o;camera_cont.o-capmgr.o|boot.o" ./gen_client_stub
