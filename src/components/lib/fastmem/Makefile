# Required variables used to drive the compilation process. It is OK
# for many of these to be empty.
#
# The library names associated with .a files output that are linked
# (via, for example, -lfastmem) into dependents. This list should be
# "fastmem" for output files such as libfastmem.a.
LIBRARY_OUTPUT = fastmem
# The .o files that are mandatorily linked into dependents. This is
# rarely used, and only when normal .a linking rules will avoid
# linking some necessary objects. This list is of names (for example,
# fastmem) which will generate fastmem.lib.o. Do NOT include the list of .o
# files here. Please note that using this list is *very rare* and
# should only be used when the .a support above is not appropriate.
OBJECT_OUTPUT =
# The path within this directory that holds the .h files for
# dependents to compile with (./ by default). Will be fed into the -I
# compiler arguments.
INCLUDE_PATHS = .
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
#
# 1. Those that are Composite-specific, and define and use the
#    Composite libraries.  These should include Makefile.lib in this
#    directory.
#
# 2. Those that are libraries from outside of Composite that already
#    have their own build process and system.  These should include
#    Makefile.extlib in this directory.
#
# By default, we include the Composite library Makefile.
include Makefile.lib
