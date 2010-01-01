MAKEFLAGS=--no-print-directory -I$(PWD)

all: kern
comps: 
	$(info )
	$(info ***************************************************)
	$(info *************** Building Components ***************)
	$(info ***************************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) -C components
kern: comps
	$(info )
	$(info ***********************************************)
	$(info *************** Building Kernel ***************)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) -C platform

cp:
	$(MAKE) $(MAKEFLAGS) -C platform cp
	$(MAKE) $(MAKEFLAGS) -C components cp

clean:
	$(MAKE) $(MAKEFLAGS) -C platform clean
	$(MAKE) $(MAKEFLAGS) -C components clean

fresh:  
	$(MAKE) $(MAKEFLAGS) -C platform fresh
	$(MAKE) $(MAKEFLAGS) -C components fresh
