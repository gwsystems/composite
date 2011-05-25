C_OBJS=pgfault.o
ASM_OBJS=
COMPONENT=pfs.o
INTERFACES=pgfault
DEPENDENCIES=sched printc stkmgr
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
