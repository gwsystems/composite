C_OBJS=sched_config.o
ASM_OBJS=
COMPONENT=schedconf.o
INTERFACES=sched_conf
DEPENDENCIES=printc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
 $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
