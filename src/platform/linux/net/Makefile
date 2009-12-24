include /home/gparmer/research/composite/src/Makefile.inc

obj-m := cnet.o
cnet-objs := cosnet.o

default: module
	$(MAKE) -C util

module:
	$(MAKE) -C $(LDIR) SUBDIRS=$(shell pwd) modules

cp:
	cp cnet.ko $(TRANS_DIR)
	$(MAKE) -C util cp

clean:
	rm -rf *.ko *.mod.c *.o *.mod.o .*.cmd .tmp_versions test Module.symvers
	$(MAKE) -C util clean
