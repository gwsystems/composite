# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=cFE_booter.o
INTERFACES=
DEPENDENCIES=
IF_LIB:=./composite_cFE.o $(wildcard test/*.o)
ADDITIONAL_LIBS=-lcobj_format -lcos_kernel_api -lcos_defkernel_api -lheap

include ../../Makefile.subsubdir
CFLAGS += -I./gen -I ./test/shared $(CPPFLAGS)

MANDITORY_LIB=simple_stklib.o
