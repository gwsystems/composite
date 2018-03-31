# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=ds_app.o
INTERFACES=
DEPENDENCIES=cFE
IF_LIB:=./cFE_ds_app.o ./cFE_ds_cmds.o ./cFE_ds_file.o ./cFE_ds_table.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
