# Order of which subdirectories are built first matters here.
# Interface files rely on library files, and component implementations
# rely on both the interfaces and the libs
MAKEOPTIONS=-I$(shell pwd)

ENABLE_STK:=${shell grep ENABLE_STACK_MANAGER include/cos_stkmgr_configure.h | awk '{print $$3 }' }

#$(info ENABLE_STK = "${ENABLE_STK}")

ifeq ($(ENABLE_STK), 1)
export ENABLE_STACK_MANAGER:=1
endif

ifdef ENABLE_STACK_MANAGER
 $(info ####### Stack Manager is Enabled #######)
else
 $(info ####### Stack Manager is Disabled #######)
endif

all:
	$(MAKE) $(MAKEOPTIONS) -C lib
	$(MAKE) $(MAKEOPTIONS) -C interface
	$(MAKE) $(MAKEOPTIONS) -C implementation

cp:
	$(MAKE) $(MAKEOPTIONS) -C implementation cp

clean:
	$(MAKE) $(MAKEOPTIONS) -C lib clean
	$(MAKE) $(MAKEOPTIONS) -C interface clean
	$(MAKE) $(MAKEOPTIONS) -C implementation clean

distclean:
	$(MAKE) $(MAKEOPTIONS) -C lib distclean

init:
	$(MAKE) $(MAKEOPTIONS) -C lib init
	$(MAKE) $(MAKEOPTIONS) -C interface init
	$(MAKE) $(MAKEOPTIONS) -C implementation init
