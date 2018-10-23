#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
cp hier_fprr.o vmboot.o
./cos_linker "llboot.o, ;capmgr.o, ;micro_asyncping.o, ;micro_asyncpong.o, ;*boot.o, ;*vmboot.o,'c10':boot.o-capmgr.o;vmboot.o-capmgr.o|[parent_]boot.o;micro_asyncping.o-capmgr.o|boot.o;micro_asyncpong.o-capmgr.o|vmboot.o" ./gen_client_stub
