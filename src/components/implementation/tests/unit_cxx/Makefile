C_OBJS=cxx.o
COMPONENT=cxx_test.o
INTERFACES=
DEPENDENCIES=cbuf_mgr printc sched valloc mem_mgr_large torrent evt timed_blk lock
IF_LIB=./libstdc++.a ./libsupc++.a 
OBJLIBS += $(POSIX_LIB)
ADDITIONAL_LIBS=-lcobj_format -lcos_kernel_api

pre: first all
first:
	$(info |     [CP]   copying libsupc++.a from libcxx)
	cp $(LIBSUPCXXOBJ) .
	$(info |     [CP]   copying libstdc++.a from libcxx)
	cp $(LIBSTDCXXOBJ) .

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o

