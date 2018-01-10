C_OBJS=dpdk_init.o
ASM_OBJS=
COMPONENT=cos_dpdk.o
INTERFACES=
DEPENDENCIES=
IF_LIB=
OBJLIBS+= $(POSIX_LIB)
ADDITIONAL_LIBS=-lcobj_format -lcos_defkernel_api -lcos_kernel_api -lsl -lheap -lsl_mod_fprr -lsl_thd_static_backend -lcos_defkernel_api -ldpdk -lrte_eal -lrte_ring

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
