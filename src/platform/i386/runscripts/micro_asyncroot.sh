#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
./cos_linker "llboot.o, ;capmgr.o, ;micro_asyncping.o, ;micro_asyncpong.o, ;*boot.o, :boot.o-capmgr.o;micro_asyncping.o-capmgr.o|boot.o;micro_asyncpong.o-capmgr.o|boot.o" ./gen_client_stub
