# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=f42.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./app_fw_utils.o ./cFE_ctrltbl.o  ./cFE_f42_adp.o ./cFE_fswkit.o ./cFE_orbkit.o ./cmdmgr.o ./jsmn.o ./tblmgr.o ./cFE_cfs_utils.o ./cFE_dcmkit.o ./cFE_f42_app.o ./cFE_mathkit.o ./cFE_ThreeAxisFsw.o ./json.o ./osk_app_fw_init.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
