C_OBJS=camera.o
ASM_OBJS=
COMPONENT=camera_cont.o
INTERFACES=camera
DEPENDENCIES=
IF_LIB:=./greenroomba.o
ADDITIONAL_LIBS= -lcos_kernel_api -lcobj_format -lsl -lheap -lsl_thd_static_backend -lsl_lock -lsl_mod_fprr -lcos_defkernel_api
OBJLIBS += $(POSIX_LIB)

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o

greenroomba.o: greenroomba.jpg
	ld -r -b binary -o greenroomba.o greenroomba.jpg

