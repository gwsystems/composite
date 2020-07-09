#MAKEFLAGS=--no-print-directory --section-alignment 0x1000 -I$(PWD)
# Note: can't use $(PWD) here if make -Ced from elsewhere.
MAKEOPTIONS=--no-print-directory -I$(shell pwd)
PLAT_FILE=.PLATFORM_ID

.PHONY: default all composer component comps platclean plat cpplat clean distclean init update config

default: | all

all: composer comps plat

composer:
	cd composer/ ; cargo build

component:
	$(MAKE) $(MAKEOPTIONS) -C components component

comps:
	$(info )
	$(info ***********************************************)
	$(info ************* Building Components *************)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEOPTIONS) -C components

platclean:
	$(MAKE) $(MAKEOPTIONS) PLATFORM=$(shell cat $(PLAT_FILE)) -C platform clean

plat: platclean
	$(info )
	$(info ***********************************************)
	$(info ***** Building $(shell cat $(PLAT_FILE)) Platform and Kernel *******)
	$(info ***********************************************)
	$(info )
	$(MAKE) $(MAKEOPTIONS) PLATFORM=$(shell cat $(PLAT_FILE)) -C platform

cpplat:
	$(MAKE) $(MAKEOPTIONS) PLATFORM=$(shell cat $(PLAT_FILE)) -C platform cp

clean: platclean
	$(MAKE) $(MAKEOPTIONS) -C components clean


distclean: clean
	$(MAKE) $(MAKEOPTIONS) -C components distclean
	@rm -f `pwd`/../transfer/*

init:
	test -f $(PLAT_FILE) || $(MAKE) $(MAKEOPTIONS) config
	$(MAKE) $(MAKEOPTIONS) -C components init
	$(MAKE) $(MAKEOPTIONS) PLATFORM=$(shell cat $(PLAT_FILE)) -C platform init

update:
	git submodule update --init --recursive

config-gen: update
	@pwd | sed 's/\(\/[a-zA-Z0-9]*\/[a-zA-Z0-9]*\/\).*/HOME_DIR=\1/' > Makefile.cosconfig
	@echo "CODE_DIR=`pwd`" >> Makefile.cosconfig
	@echo "TRANS_DIR=\$$(CODE_DIR)/../transfer" >> Makefile.cosconfig
	@mkdir -p `pwd`/../transfer
	@echo "LDIR=\$$(CODE_DIR)/linux-2.6.36/" >> Makefile.cosconfig
	@echo "TEST_DIR=/root/experiments/" >> Makefile.cosconfig

# default platform is i386
config: config-i386

# override the default linux chal link
config-i386: config-gen
	@cd kernel/include/ ; rm -f chal ; ln -s ../../platform/i386/chal/ chal
	@echo "i386" > $(PLAT_FILE)
	@echo "ARCH_CFLAGS = -m32 -D__x86__" >  Makefile.arch
	@echo "ARCH_ASFLAGS= -m32 -D__x86__ -D__ASM__" >> Makefile.arch
	@echo "ARCH_LDFLAGS=-melf_i386"      >> Makefile.arch
	@echo "Configuring the system with clock speed and directory structure."
	@cat /proc/cpuinfo | grep "model name" -m 1 | sed 's/.*\([0-9]\.[0-9]*\).*/\#define CPU_GHZ \1/' > ./kernel/include/chal/shared/cpu_ghz.h
	@if [ -z "`cat /proc/cpuinfo | grep \"physical id\"`" ]; then echo "#define NUM_CPU_SOCKETS 1" >> ./kernel/include/chal/shared/cpu_ghz.h; else cat /proc/cpuinfo | grep "physical id" | sort | uniq | wc -l | sed 's/.*\([0-9]\).*/\#define NUM_CPU_SOCKETS \1/' >> ./kernel/include/chal/shared/cpu_ghz.h; fi
	@echo "Please view Makefile.cosconfig and kernel/include/chal/shared/cpu_ghz.h to make sure they're accurate."
	@echo "Do _not_ 'git add' either of these files."
	@echo "#define COS_PLATFORM I386" >> ./kernel/include/chal/shared/cpu_ghz.h

config-armv7a: config-gen
	@cd kernel/include/ ; rm -f chal ; ln -s ../../platform/armv7a/chal/ chal
	@echo "armv7a" > $(PLAT_FILE)
	@echo "ARCH_CFLAGS = -march=armv7-a -D__arm__" >  Makefile.arch
	@echo "ARCH_ASFLAGS= -march=armv7-a -D__arm__ -D__ASM__" >> Makefile.arch
	@echo "ARCH_LDFLAGS="                          >> Makefile.arch
	@echo "Configuring the system with clock speed and directory structure."
	@cat /proc/cpuinfo | grep "model name" -m 1 | sed 's/.*\([0-9]\.[0-9]*\).*/\#define CPU_GHZ \1/' > ./kernel/include/chal/shared/cpu_ghz.h
	@if [ -z "`cat /proc/cpuinfo | grep \"physical id\"`" ]; then echo "#define NUM_CPU_SOCKETS 1" >> ./kernel/include/chal/shared/cpu_ghz.h; else cat /proc/cpuinfo | grep "physical id" | sort | uniq | wc -l | sed 's/.*\([0-9]\).*/\#define NUM_CPU_SOCKETS \1/' >> ./kernel/include/chal/shared/cpu_ghz.h; fi
	@echo "Please view Makefile.cosconfig and kernel/include/chal/shared/cpu_ghz.h to make sure they're accurate."
	@echo "Do _not_ 'git add' either of these files."
	@echo "#define COS_PLATFORM ARMV7A" >> ./kernel/include/chal/shared/cpu_ghz.h

run:
ifeq ($(RUNSCRIPT),)
	@echo "Please provide a runscript: make RUNSCRIPT=rs.sh run"
	@exit 1
endif

	@cd ../transfer && echo "Running" $(RUNSCRIPT) && ./qemu.sh $(RUNSCRIPT)
