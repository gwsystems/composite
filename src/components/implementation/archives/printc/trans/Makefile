C_OBJS=print_comp.o
ASM_OBJS=
COMPONENT=printl.o
INTERFACES=printc
DEPENDENCIES=qfault
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
