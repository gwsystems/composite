# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=lc.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./app_fw_utils.o ./cmdmgr.o ./jsmn.o ./tblmgr.o ./json.o ./osk_app_fw_init.o ./cFE_lc_app.o ./cFE_lc_action.o ./cFE_lc_cmds.o ./cFE_lc_custom.o ./cFE_lc_watch.o ./cFE_cfs_utils.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
