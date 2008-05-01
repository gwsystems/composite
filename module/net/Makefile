obj-m := cnet.o
cnet-objs := cosnet.o
KDIR=/scratch/gabep1/linux-2.6.22-current
#/scratch/gabep1/linux-2.6.13.noglobals
#/scratch/gabep1/other/linux-2.6.13
MAKE=make
TRANS_DIR=/home/grad3/gabep1/transfer/

default: module
	$(MAKE) -C util

module:
	$(MAKE) -C $(KDIR) SUBDIRS=$(shell pwd) modules

copy:
	cp cnet.ko $(TRANS_DIR)
	$(MAKE) -C util cp

clean:
	rm -rf *.ko *.mod.c *.o *.mod.o .*.cmd .tmp_versions test
	$(MAKE) -C util clean
