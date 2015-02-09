C_OBJS=posix_test.o
ASM_OBJS=
COMPONENT=posix_t.o
INTERFACES=
DEPENDENCIES=sched printc timed_blk torrent cbuf_mgr evt mem_mgr_large valloc lock
IF_LIB=./cos_libccv.a $(DIETDIR)/bin-i386/libm.a
OBJLIBS += $(POSIX_LIB)

pre: first all
first:
	$(info |     [CP]   copying cos_libccv.a from libccv)
	@cp $(CCVOBJ) .

include ../../Makefile.subsubdir
