C_OBJS=camera.o jpeg.o
ASM_OBJS=
COMPONENT=camera_cont.o
INTERFACES=camera
DEPENDENCIES=shdmem
IF_LIB=./jpeg.o
#ADDITIONAL_LIBS= -lcos_kernel_api -lcobj_format -lsl -lheap -lsl_thd_static_backend =lsl_lock -lsl_mod_fprr -lcos_defkernel_api
ADDITIONAL_LIBS= -lcos_kernel_api -lcobj_format -lsl -lheap -lsl_thd_static_backend -lsl_lock -lsl_mod_fprr -lcos_defkernel_api
OBJLIBS += $(POSIX_LIB)

PHONY: jpeg.o

jpeg.o: image.jpg
	ld -r -b binary -o jpeg.o image.jpg

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
