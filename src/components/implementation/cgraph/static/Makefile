C_OBJS=comp_graph.o
ASM_OBJS=
COMPONENT=cg.o
INTERFACES=cgraph
DEPENDENCIES=sched
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif

