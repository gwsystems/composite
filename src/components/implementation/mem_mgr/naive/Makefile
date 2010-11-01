C_OBJS=mem_man.o
ASM_OBJS=
COMPONENT=mm.o
INTERFACES=mem_mgr
DEPENDENCIES=printc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
