C_OBJS=deferrable.o
ASM_OBJS=
COMPONENT=ds.o
INTERFACES=sched sched_hier
DEPENDENCIES=printc sched_hier
IF_LIB=../complib.o
FN_PREPEND=parent_

include Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
