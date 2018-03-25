#!/bin/sh

cp llboot_comp.o llboot.o
cp root_fprr.o boot.o
cp hier_fprr.o hier_fprr1.o
cp hier_fprr.o hier_fprr2.o
cp hier_fprr.o hier_fprr3.o
cp unit_schedcomp_test.o unit_schedcomp_test1.o
cp unit_schedcomp_test.o unit_schedcomp_test2.o
cp unit_schedcomp_test.o unit_schedcomp_test3.o
cp unit_schedcomp_test.o unit_schedcomp_test4.o
./cos_linker "llboot.o, ;*hier_fprr1.o, ;capmgr.o, ;*hier_fprr2.o, ;*boot.o, ;*hier_fprr3.o, ;unit_schedcomp_test1.o, ;unit_schedcomp_test2.o, ;unit_schedcomp_test3.o, ;unit_schedcomp_test4.o, :boot.o-capmgr.o;hier_fprr1.o-capmgr.o|[parent_]boot.o;hier_fprr2.o-capmgr.o|[parent_]boot.o;hier_fprr3.o-capmgr.o|[parent_]hier_fprr1.o;unit_schedcomp_test1.o-boot.o;unit_schedcomp_test2.o-hier_fprr1.o;unit_schedcomp_test3.o-hier_fprr2.o;unit_schedcomp_test4.o-hier_fprr3.o" ./gen_client_stub
