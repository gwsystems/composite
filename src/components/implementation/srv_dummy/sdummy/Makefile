C_OBJS=
ASM_OBJS=
COMPONENT=srvdummy_server.o
INTERFACES=sched schedinit srv_dummy
DEPENDENCIES=capmgr sched schedinit
IF_LIB=
FN_PREPEND=parent_
ADDITIONAL_LIBS=$(LIBSLCAPMGR) -lsl_thd_static_backend -lsl_mod_fprr

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
