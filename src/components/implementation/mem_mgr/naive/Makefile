C_OBJS=mem_man.o
ASM_OBJS=cos_asm_scheduler.o
COMPONENT=mm.o
INTERFACES=mem_mgr sched
DEPENDENCIES=printc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
