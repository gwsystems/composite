# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=cs.o
INTERFACES=
DEPENDENCIES=cFE capmgr
IF_LIB:=./cFE_cs_app.o ./cFE_cs_cmds.o ./cFE_cs_compute.o ./cFE_cs_eeprom_cmds.o ./cFE_cs_app_cmds.o ./cFE_cs_memory_cmds.o ./cFE_cs_table_cmds.o ./cFE_cs_table_processing.o ./cFE_cs_utils.o
ADDITIONAL_LIBS=-lcos_kernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
