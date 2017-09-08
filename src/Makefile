MAKEFLAGS=--no-print-directory --section-alignment 0x1000 -I$(PWD)
#$(info Make flags $(MAKEFLAGS))

default: | all cp

all: comps plat

comps:
	$(info )
	$(info ***********************************************)
	$(info ************* Building Components *************)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) -C components

plat:
	$(info )
	$(info ***********************************************)
	$(info ***** Building $(shell cat PLATFORM_ID) Platform and Kernel *******)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEFLAGS) PLATFORM=$(shell cat PLATFORM_ID) -C platform

cpplat:
	$(MAKE) $(MAKEFLAGS) PLATFORM=$(shell cat PLATFORM_ID) -C platform cp

cp: cpplat
	$(MAKE) $(MAKEFLAGS) -C components cp

clean:
	$(MAKE) $(MAKEFLAGS) PLATFORM=$(shell cat PLATFORM_ID) -C platform clean
	$(MAKE) $(MAKEFLAGS) -C components clean


distclean: clean
	$(MAKE) $(MAKEFLAGS) -C components distclean
	@rm -f `pwd`/../transfer/*

init:
	test -f PLATFORM_ID || $(MAKE) $(MAKEFLAGS) config
	$(MAKE) $(MAKEFLAGS) -C components init
	$(MAKE) $(MAKEFLAGS) PLATFORM=$(shell cat PLATFORM_ID) -C platform init

update:
	git submodule update --init --recursive

config-gen: update
	@echo "Configuring the system with clock speed and directory structure."
	@echo "Please view Makefile.cosconfig and kernel/include/shared/cpu_ghz.h to make sure they're accurate."
	@echo "Do _not_ 'git add' either of these files."
	@cat /proc/cpuinfo | grep "model name" -m 1 | sed 's/.*\([0-9]\.[0-9]*\).*/\#define CPU_GHZ \1/' > ./kernel/include/shared/cpu_ghz.h
	@if [ -z "`cat /proc/cpuinfo | grep \"physical id\"`" ]; then echo "#define NUM_CPU_SOCKETS 1" >> ./kernel/include/shared/cpu_ghz.h; else cat /proc/cpuinfo | grep "physical id" | sort | uniq | wc -l | sed 's/.*\([0-9]\).*/\#define NUM_CPU_SOCKETS \1/' >> ./kernel/include/shared/cpu_ghz.h; fi
	@pwd | sed 's/\(\/[a-zA-Z0-9]*\/[a-zA-Z0-9]*\/\).*/HOME_DIR=\1/' > Makefile.cosconfig
	@echo "CODE_DIR=`pwd`" >> Makefile.cosconfig
	@echo "TRANS_DIR=\$$(CODE_DIR)/../transfer" >> Makefile.cosconfig
	@mkdir -p `pwd`/../transfer
	@echo "LDIR=\$$(CODE_DIR)/linux-2.6.36/" >> Makefile.cosconfig
	@echo "TEST_DIR=/root/experiments/" >> Makefile.cosconfig

# default platform is i386
config: config-i386

config-linux: config-gen
	@echo "#define COS_PLATFORM LINUX" >> ./kernel/include/shared/cpu_ghz.h
	@echo "#define COS_LINUX" >> ./kernel/include/shared/cpu_ghz.h
	@cd kernel/include/ ; rm -f chal ; ln -s ../../platform/linux/module/chal/ chal
	@echo "linux" > PLATFORM_ID

# override the default linux chal link
config-i386: config-gen
	@echo "#define COS_PLATFORM I386" >> ./kernel/include/shared/cpu_ghz.h
	@cd kernel/include/ ; rm -f chal ; ln -s ../../platform/i386/chal/ chal
	@echo "i386" > PLATFORM_ID

run:
ifeq ($(RUNSCRIPT),)
	@echo "Please provide a runscript: make RUNSCRIPT=rs.sh run"
	@exit 1
endif

	@cd ../transfer && echo "Running" $(RUNSCRIPT) && ./qemu.sh $(RUNSCRIPT)

idl:
	$(MAKE) $(MAKEFLAGS) -C components idl

ltests:
	$(MAKE) $(MAKEFLAGS) -C platform ltests

cltests:
	$(MAKE) $(MAKEFLAGS) -C platform cltests
