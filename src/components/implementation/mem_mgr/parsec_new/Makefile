C_OBJS=mem_man_parsec.o
COMPONENT=mm_parsec.o
INTERFACES=mem_mgr llpong
DEPENDENCIES=
#FN_PREPEND=parent_

IF_LIB=../complib.o

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
