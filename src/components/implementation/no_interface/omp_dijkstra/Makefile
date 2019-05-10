COMPONENT=omp_dijkstra.o
INTERFACES=
DEPENDENCIES=capmgr
IF_LIB=
ADDITIONAL_LIBS=-lcobj_format -lcos_kernel_api -lcos_gomp $(LIBSLCAPMGR) -lsl_mod_fifo -lsl_thd_static_backend -lsl_lock -lcos_defkernel_api -lpart -lsl_blkpt

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o

CFLAGS += -fopenmp
