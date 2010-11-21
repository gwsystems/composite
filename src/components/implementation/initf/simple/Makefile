C_OBJS=initf.o
ASM_OBJS=
COMPONENT=init.o
INTERFACES=initf
DEPENDENCIES=sched printc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
 MANDITORY_LIB=simple_stklib.o
endif
