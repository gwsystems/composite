# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=sc.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./cFE_sc_app.o ./cFE_sc_atsrq.o ./cFE_sc_cmds.o ./cFE_sc_loads.o ./cFE_sc_rtsrq.o ./cFE_sc_state.o ./cFE_sc_utils.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
