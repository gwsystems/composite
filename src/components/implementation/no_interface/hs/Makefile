# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=hs.o
INTERFACES=
DEPENDENCIES=cFE capmgr channel
IF_LIB:=./app_fw_utils.o ./cmdmgr.o ./jsmn.o ./tblmgr.o ./json.o ./osk_app_fw_init.o ./cFE_hs_app.o ./cFE_hs_cmds.o ./cFE_hs_custom.o ./cFE_hs_monitors.o ./cFE_cfs_utils.o
ADDITIONAL_LIBS=-lcos_kernel_api -lacom_client

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
