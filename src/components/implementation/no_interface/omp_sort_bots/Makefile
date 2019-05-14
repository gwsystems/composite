COMPONENT=omp_sort_bots.o
INTERFACES=
DEPENDENCIES=capmgr
IF_LIB=
ADDITIONAL_LIBS=-lcobj_format -lcos_kernel_api -lcos_gomp $(LIBSLCAPMGR) -lsl_mod_part_fifo -lsl_thd_static_backend -lsl_lock -lcos_defkernel_api -lpart -lsl_blkpt

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o

CFLAGS += -fopenmp
# if tied tasks are required
CFLAGS += -DFORCE_TIED_TASKS

OMPC_FINAL_FLAGS=

# one per compilation or none
#CFLAGS += -DMANUAL_CUTOFF
#CFLAGS += -DIF_CUTOFF
#CFLAGS += -DFINAL_CUTOFF $(OMPC_FINAL_FLAGS) 
