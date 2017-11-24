ASM_OBJS=start.o
COMPONENT=noiso_rumpboot.o
INTERFACES=
DEPENDENCIES=shdmem
IF_LIB=
ADDITIONAL_LIBS=-lcobj_format -lcos_defkernel_api -lcos_kernel_api -lsl -lheap -lsl_mod_fprr -lsl_thd_static_backend
OBJLIBS += $(POSIX_LIB)

include ../../Makefile.subsubdir

MANDITORY_LIB=simple_stklib.o
