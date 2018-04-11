C_OBJS=camera.o
ASM_OBJS=
COMPONENT=camera_cont.o
INTERFACES=camera
DEPENDENCIES=capmgr sched
IF_LIB:=./greenroomba.o
ADDITIONAL_LIBS= -lcos_kernel_api -lcobj_format -lcos_defkernel_api

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o

greenroomba.o: greenroomba.jpg
	ld -r -b binary -o greenroomba.o greenroomba.jpg
