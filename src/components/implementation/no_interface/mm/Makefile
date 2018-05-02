# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=mm.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./cFE_mm_app.o ./cFE_mm_dump.o ./cFE_mm_load.o ./cFE_mm_mem16.o ./cFE_mm_mem32.o ./cFE_mm_mem8.o ./cFE_mm_utils.o ./cFE_cfs_utils.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
