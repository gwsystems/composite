C_OBJS=robot_cont.o
ASM_OBJS=
COMPONENT=robot_controller.o
INTERFACES=robot_cont
#DEPENDENCIES=shdmem camera
DEPENDENCIES=shdmem
IF_LIB=
ADDITIONAL_LIBS= -lcos_kernel_api -lcobj_format -lsl -lheap -lsl_thd_static_backend -lsl_lock -lsl_mod_fprr -lcos_defkernel_api
OBJLIBS += $(POSIX_LIB)

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
