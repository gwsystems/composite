#!/bin/sh

cp root_fprr.o boot.o
cp llboot_comp.o llboot.o
cp cFE_booter.o cFE.o
cp rk_stubcomp.o kitcistub.o
cp rk_stubcomp.o kittostub.o
./cos_linker "llboot.o, ;capmgr.o, ;*cFE.o,'c01';*rumpcos.o,'c10';*boot.o, ;http.o, ;bm.o, ;ds.o, ;f42.o, ;fm.o, ;hc.o, ;i42.o, ;kit_sch.o, ;lc.o, ;md.o, ;mm.o, ;sc.o, ;sim.o, ;kit_ci.o,'r1';kitcistub.o,'r1';kit_to.o,'r2';kittostub.o,'r2':boot.o-capmgr.o;rumpcos.o-capmgr.o|[parent_]boot.o;http.o-rumpcos.o|capmgr.o;cFE.o-capmgr.o|[parent_]boot.o;bm.o-capmgr.o|cFE.o;ds.o-capmgr.o|cFE.o;f42.o-capmgr.o|cFE.o;fm.o-capmgr.o|cFE.o;hc.o-capmgr.o|cFE.o;i42.o-capmgr.o|cFE.o;kitcistub.o-rumpcos.o|capmgr.o;kit_ci.o-capmgr.o|cFE.o;kit_sch.o-capmgr.o|cFE.o;kittostub.o-rumpcos.o|capmgr.o;kit_to.o-capmgr.o|cFE.o;lc.o-capmgr.o|cFE.o;md.o-capmgr.o|cFE.o;mm.o-capmgr.o|cFE.o;sc.o-capmgr.o|cFE.o;sim.o-capmgr.o|cFE.o" ./gen_client_stub
