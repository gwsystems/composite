C_OBJS=omp_fft.o
ASM_OBJS=
COMPONENT=fft.o
INTERFACES=omp
DEPENDENCIES=mem_mgr_large sched valloc printc acap_mgr_intra timed_blk lock
IF_LIB=../complib.o /usr/lib/libm.a

include ../../Makefile.subsubdir

CFLAGS += -fopenmp
CFLAGS += -Wl,-Bstatic -lm -Wl,-Bdynamic #FIXME: this doesn't work. use the IF_LIB for now. 

