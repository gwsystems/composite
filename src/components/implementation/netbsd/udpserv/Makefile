C_OBJS=udpserver.c
ASM_OBJS=
COMPONENT=udpserv.o
INTERFACES=netbsd
DEPENDENCIES=rk
IF_LIB=

include ../../Makefile.subsubdir

CFLAGS += -I../../no_interface/rumpbooter/

ifeq (${ENABLE_STACK_MANAGER}, 1)
 #$(info Using Simple Stack within udpserv)
 MANDITORY_LIB=simple_stklib.o
endif
