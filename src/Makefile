MAKEFLAGS=--no-print-directory --section-alignment 0x1000 -I$(PWD)
#$(info Make flags $(MAKEFLAGS))

all: comps $(shell cat PLATFORM)

comps: 
	$(info )
	$(info ***********************************************)
	$(info ************* Building Components *************)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) -C components

linux: 
	$(info )
	$(info ***********************************************)
	$(info ***** Building Linux Platform and Kernel ******)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) PLATFORM=linux -C platform

i386: 
	$(info )
	$(info ***********************************************)
	$(info ***** Building i386 Platform and Kernel *******)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) PLATFORM=i386 -C platform

cpplat:
	$(MAKE) $(MAKEFLAGS) -C platform cp

cp: cpplat
	$(MAKE) $(MAKEFLAGS) -C components cp

clean:
	$(MAKE) $(MAKEFLAGS) -C platform clean
	$(MAKE) $(MAKEFLAGS) -C components clean


distclean: clean
	$(MAKE) $(MAKEFLAGS) -C components distclean	

init:
	$(MAKE) $(MAKEFLAGS) -C components init
	$(MAKE) $(MAKEFLAGS) -C platform init

config-gen:
	@echo "Configuring the system with clock speed and directory structure."
	@echo "Please view Makefile.cosconfig and kernel/include/shared/cpu_ghz.h to make sure they're accurate."
	@echo "Do _not_ 'git add' either of these files."
	@cat /proc/cpuinfo | grep "model name" -m 1 | sed 's/.*\([0-9]\.[0-9]*\).*/\#define CPU_GHZ \1/' > ./kernel/include/shared/cpu_ghz.h
	@if [ -z "`cat /proc/cpuinfo | grep \"physical id\"`" ]; then echo "#define NUM_CPU_SOCKETS 1" >> ./kernel/include/shared/cpu_ghz.h; else cat /proc/cpuinfo | grep "physical id" | sort | uniq | wc -l | sed 's/.*\([0-9]\).*/\#define NUM_CPU_SOCKETS \1/' >> ./kernel/include/shared/cpu_ghz.h; fi
	@pwd | sed 's/\(\/[a-zA-Z0-9]*\/[a-zA-Z0-9]*\/\).*/HOME_DIR=\1/' > Makefile.cosconfig
	@echo "CODE_DIR=\$$(HOME_DIR)/research/" >> Makefile.cosconfig
	@echo "TRANS_DIR=\$$(HOME_DIR)/transfer/" >> Makefile.cosconfig
	@echo "LDIR=\$$(CODE_DIR)/linux-2.6.36/" >> Makefile.cosconfig
	@echo "TEST_DIR=/root/experiments/" >> Makefile.cosconfig

# default platform is linux
config: config-linux

config-linux: config-gen
	@echo "#define COS_PLATFORM LINUX" >> ./kernel/include/shared/cpu_ghz.h
	@echo "#define COS_LINUX" >> ./kernel/include/shared/cpu_ghz.h
	@cd kernel/include/ ; rm -f chal ; ln -s ../../platform/linux/module/chal/ chal
	@echo "linux" > PLATFORM

# override the default linux chal link
config-i386: config-gen
	@echo "#define COS_PLATFORM I386" >> ./kernel/include/shared/cpu_ghz.h
	@cd kernel/include/ ; rm -f chal ; ln -s ../../platform/i386/chal/ chal
	@echo "i386" > PLATFORM

idl:
	$(MAKE) $(MAKEFLAGS) -C components idl

ltests:
	$(MAKE) $(MAKEFLAGS) -C platform ltests

cltests:
	$(MAKE) $(MAKEFLAGS) -C platform cltests
