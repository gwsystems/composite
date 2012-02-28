C_OBJS=mem_man.o
ASM_OBJS=
COMPONENT=mh.o
INTERFACES=mem_mgr
DEPENDENCIES=printc mem_mgr
FN_PREPEND=parent_

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
