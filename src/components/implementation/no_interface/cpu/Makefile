C_OBJS=spin.o
ASM_OBJS=
COMPONENT=cpu.o
INTERFACES=
DEPENDENCIES=sched printc periodic_wake
ADDITIONAL_LIBS=-lsconf

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif

