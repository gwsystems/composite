#C_OBJS=../*.c
COMPONENT=rk_default.o
INTERFACES=rk sched schedinit
DEPENDENCIES=capmgr schedinit
IF_LIB=
FN_PREPEND=parent_
ADDITIONAL_LIBS=$(LIBSLCAPMGR) -lsl_mod_fp -lsl_thd_static_backend -lheap

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
