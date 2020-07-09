# Required variables used to drive the compilation process. It is OK
# for many of these to be empty.
#
# The library names associated with .a files output that are linked
# (via, for example, -lLIBC) into dependents. This list should be
# "LIBC" for output files such as libLIBC.a.
#
# NOTE: libc is special-cased and the compilation is orchestrated
# directly. It should have no dependencies.
LIBRARY_OUTPUT = c crypt m thread resolv rt
# The .o files that are mandatorily linked into dependents. This is
# rarely used, and only when normal .a linking rules will avoid
# linking some necessary objects. This list is of names (for example,
# LIBC) which will generate LIBC.lib.o. Do NOT include the list of .o
# files here. Please note that using this list is *very rare* and
# should only be used when the .a support above is not appropriate.
OBJECT_OUTPUT =
# The path within this directory that holds the .h files for
# dependents to compile with (./ by default). Will be fed into the -I
# compiler arguments.
INCLUDE_PATHS = musl-1.1.11/include/
# The interfaces this component is dependent on for compilation (this
# is a list of directory names in interface/)
INTERFACE_DEPENDENCIES =
# The library dependencies this component is reliant on for
# compilation/linking (this is a list of directory names in lib/)
LIBRARY_DEPENDENCIES =
# Note: Both the interface and library dependencies should be
# *minimal*. That is to say that removing a dependency should cause
# the build to fail. The build system does not validate this
# minimality; that's on you!

# There are two different *types* of Makefiles for libraries.
# 1. Those that are Composite-specific, and simply need an easy way to
#    compile and itegrate their code.
# 2. Those that aim to integrate external libraries into
#    Composite. These focus on "driving" the build process of the
#    external library, then pulling out the resulting files and
#    directories. These need to be flexible as all libraries are
#    different.

# Type 1, Composite library: This is the default Makefile for
# libraries written for composite. Get rid of this if you require a
# custom Makefile (e.g. if you use an existing
# (non-composite-specific) library. An example of this is `kernel`.
## include Makefile.lib

## Type 2, external library: If you need to specialize the Makefile
## for an external library, you can add the external code as a
## subdirectory, and drive its compilation, and integration with the
## system using a specialized Makefile. The Makefile must generate
## lib$(LIBRARY_OUTPUT).a and $(OBJECT_OUTPUT).lib.o, and have all of
## the necessary include paths in $(INCLUDE_PATHS).
##
## To access the Composite Makefile definitions, use the following. An
## example of a Makefile written in this way is in `ps/`.
#
# include Makefile.src Makefile.comp Makefile.dependencies
# .PHONY: all clean init distclean
## Fill these out with your implementation
# all:
# clean:
#
## Default rules:
# init: clean all
# distclean: clean

MUSLDIR=musl-1.1.11/
MUSLINC=$(MUSLDIR)/include/
MUSLLIB=$(MUSLDIR)/lib/

include Makefile.src Makefile.comp Makefile.dependencies

.PHONY: all clean init distclean armv7_init i386_init
all:
clean:

armv7_init:
	$(info Building libc (musl) for arm...)
	cd $(MUSLDIR); ./configure "CC=arm-none-eabi-gcc" "CFLAGS=-march=armv7-a -O3" "LDFLAGS=" --disable-shared --target=arm; cd ..

i386_init:
	$(info Building libc (musl) for i386...)
	cd $(MUSLDIR); ./configure "CFLAGS=-m32 -O3" "LDFLAGS=-Wl,-melf_i386" --disable-shared --target=i386; cd ..

libc.READY:
	make $(PLATFORM)_init
	make -C $(MUSLDIR)
	make -C $(MUSLDIR) install
	cp $(MUSLLIB)*.a .
	touch libc.READY
	rm libutil.a

init: libc.READY

distclean:
	make -C $(MUSLDIR) distclean
	rm -rf *.a
	rm -f libc.READY
