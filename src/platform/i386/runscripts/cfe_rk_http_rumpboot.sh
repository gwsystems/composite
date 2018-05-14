#!/bin/sh

cp root_fprr.o boot.o
cp llboot_comp.o llboot.o
cp cFE_booter.o cFE.o
./cos_linker "llboot.o, ;capmgr.o, ;*rumpcos.o, ;http.o, ;*boot.o, ;rk_stubcomp.o, ;*cFE.o, ;ds.o, ;fm.o, ;sc.o, ;hs.o, ;mm.o, ;sch_lab.o, :boot.o-capmgr.o;rumpcos.o-capmgr.o|[parent_]boot.o;http.o-rumpcos.o|capmgr.o;rk_stubcomp.o-rumpcos.o|capmgr.o;cFE.o-capmgr.o|[parent_]boot.o;ds.o-capmgr.o|cFE.o;fm.o-capmgr.o|cFE.o;sc.o-capmgr.o|cFE.o;hs.o-capmgr.o|cFE.o;mm.o-capmgr.o|cFE.o;sch_lab.o-capmgr.o|cFE.o" ./gen_client_stub
