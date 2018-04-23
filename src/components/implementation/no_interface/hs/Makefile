# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=hs_app.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./cFE_hs_app.o ./cFE_hs_cmds.o ./cFE_hs_custom.o ./cFE_hs_monitors.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
