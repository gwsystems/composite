C_OBJS=fd.o
ASM_OBJS=
COMPONENT=face_detect.o
INTERFACES=
DEPENDENCIES=sched printc timed_blk torrent cbufp evt mem_mgr_large valloc lock
IF_LIB=./cos_libccv.a $(DIETDIR)/bin-i386/libm.a
OBJLIBS += $(POSIX_LIB)

pre: first all
first:
	$(info |     [CP]   copying cos_libccv.a from libccv)
	@cp $(CCVOBJ) .

include ../../Makefile.subsubdir
