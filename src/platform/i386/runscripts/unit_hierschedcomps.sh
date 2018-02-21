#!/bin/sh

cp llboot_comp.o llboot.o
cp hier_fprr.o 3_2_hier_fprr.o
cp hier_fprr.o 4_2_hier_fprr.o
cp hier_fprr.o 5_3_hier_fprr.o
cp unit_schedcomp_test.o _2_unit_schedcomp_test.o
cp unit_schedcomp_test.o _3_unit_schedcomp_test.o
cp unit_schedcomp_test.o _4_unit_schedcomp_test.o
cp unit_schedcomp_test.o _5_unit_schedcomp_test.o
./cos_linker "llboot.o, ;resmgr.o, ;root_fprr.o, ;3_2_hier_fprr.o, ;4_2_hier_fprr.o, ;5_3_hier_fprr.o, ;_2_unit_schedcomp_test.o, ;_3_unit_schedcomp_test.o, ;_4_unit_schedcomp_test.o, ;_5_unit_schedcomp_test.o, :root_fprr.o-resmgr.o;3_2_hier_fprr.o-resmgr.o|[parent_]root_fprr.o;4_2_hier_fprr.o-resmgr.o|[parent_]root_fprr.o;5_3_hier_fprr.o-resmgr.o|[parent_]3_2_hier_fprr.o;_2_unit_schedcomp_test.o-root_fprr.o;_3_unit_schedcomp_test.o-3_2_hier_fprr.o;_4_unit_schedcomp_test.o-4_2_hier_fprr.o;_5_unit_schedcomp_test.o-5_3_hier_fprr.o" ./gen_client_stub
