MAKEOPTIONS=-I$(shell pwd)

# Order of which subdirectories are built first matters here.
# Interface files rely on library files, and component implementations
# rely on both the interfaces and the libs

all:
	$(MAKE) $(MAKEOPTIONS) -C lib
	$(MAKE) $(MAKEOPTIONS) -C interface
	$(MAKE) $(MAKEOPTIONS) -C implementation

clean:
	$(MAKE) $(MAKEOPTIONS) -C interface clean
	$(MAKE) $(MAKEOPTIONS) -C implementation clean
	$(MAKE) $(MAKEOPTIONS) -C lib clean

distclean:
	$(MAKE) $(MAKEOPTIONS) -C lib distclean

init:
	$(MAKE) $(MAKEOPTIONS) -C lib init
	$(MAKE) $(MAKEOPTIONS) -C interface init
	$(MAKE) $(MAKEOPTIONS) -C implementation init

component:
	$(MAKE) $(MAKEOPTIONS) -C implementation component

single_lib:
	$(MAKE) $(MAKEOPTIONS) -C lib single_lib

scan_libs_for_constants:
	$(MAKE) $(MAKEOPTIONS) -C lib scan_libs_for_constants