C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o
ASM_OBJS=
COMPONENT=cFE_booter.o
INTERFACES=
DEPENDENCIES=
IF_LIB=./composite_cFE.o
ADDITIONAL_LIBS=-lcobj_format -lcos_kernel_api

include ../../Makefile.subsubdir
CFLAGS += -I./gen

MANDITORY_LIB=simple_stklib.o
