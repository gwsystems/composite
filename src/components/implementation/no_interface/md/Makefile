# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=md.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./app_fw_utils.o ./cmdmgr.o ./jsmn.o ./tblmgr.o ./json.o ./osk_app_fw_init.o ./cFE_md_app.o ./cFE_md_cmds.o ./cFE_md_dwell_pkt.o ./cFE_md_dwell_tbl.o ./cFE_md_utils.o ./cFE_cfs_utils.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
