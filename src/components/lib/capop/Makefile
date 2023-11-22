# Required variables used to drive the compilation process. It is OK
# for many of these to be empty.
#
# The library names associated with .a files output that are linked
# (via, for example, -lsyscall) into dependents. This list should be
# "syscall" for output files such as libsyscall.a.
LIBRARY_OUTPUT = capop
# The .o files that are mandatorily linked into dependents. This is
# rarely used, and only when normal .a linking rules will avoid
# linking some necessary objects. This list is of names (for example,
# syscall) which will generate syscall.lib.o. Do NOT include the list of .o
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
LIBRARY_DEPENDENCIES = cos

# Type 1, Composite library: This is the default Makefile for
# libraries written for composite. Get rid of this if you require a
# custom Makefile (e.g. if you use an existing
# (non-composite-specific) library. An example of this is `kernel`.
include Makefile.lib
