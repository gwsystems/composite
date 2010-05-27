MAKEFLAGS=--no-print-directory -I$(PWD)
#$(info Make flags $(MAKEFLAGS))
#ENABLE_STACK_MANAGER := 1

ENABLE_STK:=${shell grep ENABLE_STACK_MANAGER components/include/cos_stkmgr_configure.h | awk '{print $$3 }' }


$(info ENABLE_STK = "${ENABLE_STK}")

ifeq ($(ENABLE_STK), 1)
export ENABLE_STACK_MANAGER:=1
endif

ifdef ENABLE_STACK_MANAGER 
 $(info ####### Stack Manager is Enabled ######)
else
 $(info ####### Stack Manager is Disabled ######)
endif

all: kern
comps: 
	$(info )
	$(info ***********************************************)
	$(info ************* Building Components *************)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) -C components
kern: comps
	$(info )
	$(info ***********************************************)
	$(info ******** Building Platform and Kernel *********)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) -C platform

cp:
	$(MAKE) $(MAKEFLAGS) -C platform cp
	$(MAKE) $(MAKEFLAGS) -C components cp

clean:
	$(MAKE) $(MAKEFLAGS) -C platform clean
	$(MAKE) $(MAKEFLAGS) -C components clean


distclean: clean
	$(MAKE) $(MAKEFLAGS) -C components distclean	

init:
	$(MAKE) $(MAKEFLAGS) -C components init
	$(MAKE) $(MAKEFLAGS) -C platform init
