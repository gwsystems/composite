# Order matters here.  Interface files rely on library files, and
# component implementations rely on both the interfaces and the libs
$(info components makeflag $(MAKEFLAGS))
MAKEFLAGS=-I$(shell pwd)

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
	make $(MAKEFLAGS) -C lib
	make $(MAKEFLAGS) -C interface
	make $(MAKEFLAGS) -C implementation

cp:
	make $(MAKEFLAGS) -C implementation cp

clean:
	make $(MAKEFLAGS) -C lib clean
	make $(MAKEFLAGS) -C interface clean
	make $(MAKEFLAGS) -C implementation clean

distclean:
	make $(MAKEFLAGS) -C lib distclean

init:
	make $(MAKEFLAGS) -C lib init
	make $(MAKEFLAGS) -C interface init
	make $(MAKEFLAGS) -C implementation init

idl:
	$(MAKE) $(MAKEFLAGS) -C interface idl
