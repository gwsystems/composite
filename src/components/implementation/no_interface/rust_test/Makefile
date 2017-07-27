# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=rust_test.o
INTERFACES=
DEPENDENCIES=
IF_LIB:=$(wildcard ./lib*.a)
ADDITIONAL_LIBS=-lcobj_format -lcos_defkernel_api -lcos_kernel_api -lsl -lheap -lsl_thd_static_backend -lsl_lock -lsl_mod_fprr

include ../../Makefile.subsubdir

MANDITORY_LIB=simple_stklib.o
