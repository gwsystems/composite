#!/bin/sh

cp root_fprr.o boot.o
cp llboot_comp.o llboot.o
cp cFE_booter.o cFE.o
./cos_linker "llboot.o, ;capmgr.o, ;*rumpcos.o,'cpu=01,';http.o,'cpu=01,';*boot.o, ;*cFE.o,'cpu=10,';ds.o,'cpu=10,';fm.o,'cpu=10,';sc.o,'cpu=10,';hs.o,'cpu=10,';mm.o,'cpu=10,';sch_lab.o,'cpu=10,':boot.o-capmgr.o;rumpcos.o-capmgr.o|[parent_]boot.o;http.o-rumpcos.o|capmgr.o;cFE.o-capmgr.o|[parent_]boot.o;ds.o-capmgr.o|cFE.o;fm.o-capmgr.o|cFE.o;sc.o-capmgr.o|cFE.o;hs.o-capmgr.o|cFE.o;mm.o-capmgr.o|cFE.o;sch_lab.o-capmgr.o|cFE.o" ./gen_client_stub
