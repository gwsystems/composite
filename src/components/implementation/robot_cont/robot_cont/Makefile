C_OBJS=robot_cont.o
ASM_OBJS=
COMPONENT=robot_controller.o
INTERFACES=robot_cont
DEPENDENCIES=capmgr sched netbsd camera 
IF_LIB=
ADDITIONAL_LIBS= -lcos_kernel_api -lcobj_format -lcos_defkernel_api -lsl_sched -lsl_capmgr -lsl_mod_fprr -lsl_thd_static_backend -lheap
OBJLIBS+=$(POSIX_LIB)

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
