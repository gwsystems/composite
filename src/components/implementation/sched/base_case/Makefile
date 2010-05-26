C_OBJS=base_case.o
ASM_OBJS=
COMPONENT=bc.o
INTERFACES=sched
DEPENDENCIES=printc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
 $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
