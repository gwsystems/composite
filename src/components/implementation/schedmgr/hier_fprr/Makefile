C_OBJS=sched_mgr.c sched_init.c init.c
ASM_OBJS=
COMPONENT=hier_fprr.o
INTERFACES=schedmgr schedinit
DEPENDENCIES=resmgr schedmgr schedinit
IF_LIB=
FN_PREPEND=parent_
ADDITIONAL_LIBS=-lcos_kernel_api -lsl_sched -lcos_defkernel_api -lheap -lsl_mod_fprr -lsl_thd_static_backend -lsl_resmgr 

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
