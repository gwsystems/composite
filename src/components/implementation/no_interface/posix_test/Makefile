C_OBJS=ptest.o
ASM_OBJS=
COMPONENT=posix_t.o
INTERFACES=
DEPENDENCIES=sched printc timed_blk torrent cbufp cbuf_c evt mem_mgr_large valloc lock
IF_LIB=./cos_libccv.a ./posix.a

pre: posix libccv all
posix:
	$(info |     [CP]   copying posix.a from posix)
	@cp $(POSIXOBJ) .
libccv:
	$(info |     [CP]   copying cos_libccv.a from libccv)
	@cp $(CCVOBJ) .

include ../../Makefile.subsubdir
