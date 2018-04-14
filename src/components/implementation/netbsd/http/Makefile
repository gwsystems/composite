C_OBJS=*.c
ASM_OBJS=
COMPONENT=http.o
INTERFACES=netbsd
DEPENDENCIES=rk capmgr
IF_LIB=
ADDITIONAL_LIBS=-lcos_defkernel_api -lsl_sched -lsl_capmgr -lcos_kernel_api -lsl_mod_fprr -lsl_thd_static_backend -lheap
OBJLIBS+=$(POSIX_LIB)

include ../../Makefile.subsubdir

CFLAGS += -I../../no_interface/rumpbooter/

ifeq (${ENABLE_STACK_MANAGER}, 1)
 #$(info Using Simple Stack within udpserv)
 MANDITORY_LIB=simple_stklib.o
endif
