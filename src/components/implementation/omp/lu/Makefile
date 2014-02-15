C_OBJS=omp_lu.o
ASM_OBJS=
COMPONENT=cos_lu.o
INTERFACES=omp
DEPENDENCIES=mem_mgr_large sched valloc printc par_mgr_intra timed_blk lock
IF_LIB=../complib.o

include ../../Makefile.subsubdir

CFLAGS += -fopenmp
CFLAGS += -Wl,-Bstatic -lm -Wl,-Bdynamic #FIXME: this doesn't work. use the IF_LIB for now. 

