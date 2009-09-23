include /home/gparmer/research/composite/module/Makefile.inc

obj-m := cnet.o
cnet-objs := cosnet.o

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
