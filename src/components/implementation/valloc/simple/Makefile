C_OBJS=valloc.o
ASM_OBJS=
COMPONENT=va.o
INTERFACES=valloc
DEPENDENCIES=printc mem_mgr vas_mgr cinfo sched lock
IF_LIB=

include ../../Makefile.subsubdir

ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
