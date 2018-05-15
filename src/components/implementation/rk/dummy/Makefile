C_OBJS=
ASM_OBJS=
COMPONENT=rk_dummy.o
INTERFACES=sched rk schedinit
DEPENDENCIES=capmgr channel schedinit
FN_PREPEND=parent_
IF_LIB=
ADDITIONAL_LIBS=$(LIBSLCAPMGR) -lsl_mod_fp -lsl_thd_static_backend -lheap -lsinv_client

include ../../Makefile.subsubdir

MANDITORY_LIB=simple_stklib.o
