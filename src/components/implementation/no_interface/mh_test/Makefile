C_OBJS=mhtest.o
ASM_OBJS=
COMPONENT=mht.o
INTERFACES=
DEPENDENCIES=printc sched mem_mgr
IF_LIB=
ADDITIONAL_LIBS=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif

