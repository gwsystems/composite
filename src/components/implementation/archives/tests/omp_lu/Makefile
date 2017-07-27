C_OBJS=omp_lu.o
ASM_OBJS=
COMPONENT=cos_lu.o
INTERFACES=
DEPENDENCIES=par_mgr mem_mgr_large sched valloc printc timed_blk lock

include ../../Makefile.subsubdir

CFLAGS += -fopenmp
CFLAGS += -Wl,-Bstatic -lm -Wl,-Bdynamic #FIXME: this doesn't work. use the IF_LIB for now. 

