C_OBJS=udp_eventtrace.c
ASM_OBJS=
COMPONENT=udptrace.o
#INTERFACES=netbsd
DEPENDENCIES=rk channel capmgr schedinit sched workload
IF_LIB=
ADDITIONAL_LIBS=$(LIBSLCAPMGR) -levent_trace -lsl_mod_fprr -lsl_thd_static_backend -lheap
OBJLIBS+=$(POSIX_LIB)

include ../../Makefile.subsubdir

#CFLAGS += -I../../no_interface/rumpbooter/

ifeq (${ENABLE_STACK_MANAGER}, 1)
 #$(info Using Simple Stack within udpserv)
 MANDITORY_LIB=simple_stklib.o
endif
