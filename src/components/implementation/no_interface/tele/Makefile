# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=tele.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
CFLAGS += -I../cFE_booter/gen

MANDITORY_LIB=simple_stklib.o
