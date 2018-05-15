# cp llboot_comp.o llboot.o
# cp resmgr.o mm.o
# ./cos_linker 'llboot.o, ;mm.o, ;*cFE_booter.o, ;sample_lib.o, ;sample_app.o, ;sch_lab.o, :cFE_booter.o-mm.o;sample_app.o-cFE_booter.o;sample_lib.o-cFE_booter.o;sch_lab.o-cFE_booter.o' ./gen_client_stub

cp llboot_comp.o llboot.o
cp cFE_booter.o boot.o
# ds fm sc hs mm
./cos_linker 'llboot.o, ;ds.o, ;capmgr.o, ;fm.o, ;*boot.o, ;sc.o, ;hs.o, ;mm.o, ;f42.o, ;i42.o, ;tele.o, ;sch_lab.o, :boot.o-capmgr.o;ds.o-boot.o;fm.o-boot.o;sc.o-boot.o;hs.o-boot.o;mm.o-boot.o;sch_lab.o-boot.o;f42.o-boot.o;i42.o-boot.o;tele.o-boot.o;ds.o-capmgr.o;fm.o-capmgr.o;sc.o-capmgr.o;hs.o-capmgr.o;mm.o-capmgr.o;sch_lab.o-capmgr.o;f42.o-capmgr.o;i42.o-capmgr.o;tele.o-capmgr.o' ./gen_client_stub
