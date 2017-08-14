C_OBJS=cxx.o
COMPONENT=cxx_test.o
INTERFACES=
DEPENDENCIES=
IF_LIB=./libstdc++.a ./libsupc++.a
OBJLIBS += $(POSIX_LIB)
ADDITIONAL_LIBS=-lcobj_format -lcos_kernel_api -lcos_defkernel_api -lsl -lsl_mod_fprr -lsl_thd_static_backend

pre: first all
first:
	$(info |     [CP]   copying libsupc++.a from libcxx)
	cp $(LIBSUPCXXOBJ) .
	$(info |     [CP]   copying libstdc++.a from libcxx)
	cp $(LIBSTDCXXOBJ) .

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
