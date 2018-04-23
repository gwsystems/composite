# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=fm_app.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./cFE_fm_app.o ./cFE_fm_child.o ./cFE_fm_cmds.o ./cFE_fm_cmd_utils.o cFE_fm_tbl.o ./cFE_cfs_utils.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
