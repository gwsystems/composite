# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=sample_lib.o
INTERFACES=
DEPENDENCIES=cFE
IF_LIB:=./cFE_sample_lib.o
ADDITIONAL_LIBS=-lcos_kernel_api -lcFE_emu

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
