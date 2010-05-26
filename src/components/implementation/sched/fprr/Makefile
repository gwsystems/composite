C_OBJS=fp_rr.o
ASM_OBJS=
COMPONENT=fprr.o
INTERFACES=sched
DEPENDENCIES=printc mem_mgr stack_trace sched_conf
IF_LIB=../complib.o

include Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
 $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
