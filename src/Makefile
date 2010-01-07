MAKEFLAGS=--no-print-directory -I$(PWD)

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

init:
	$(MAKE) $(MAKEFLAGS) -C platform init
	$(MAKE) $(MAKEFLAGS) -C components init

