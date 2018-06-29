# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=kit_sch.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./app_fw_utils.o ./cmdmgr.o ./jsmn.o ./tblmgr.o ./json.o ./osk_app_fw_init.o ./cFE_kit_sch_app.o ./cFE_msgtbl.o ./cFE_scheduler.o ./cFE_schtbl.o ./cFE_cfs_utils.o ./cFE_expat_init.o ./cFE_xmlparse.o ./cFE_xmlrole.o ./cFE_xmltok.o ./cFE_xmltok_impl.o ./cFE_xmltok_ns.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
