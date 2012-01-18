include Makefile.src

obj-m := trans.o
trans-objs := translator.o

default: module
	$(MAKE) -C helpers 

module:
	$(MAKE) -C $(LDIR) SUBDIRS=$(shell pwd) modules

cp:
	cp trans.ko $(TRANS_DIR)
	$(MAKE) -C helpers cp

clean:
	rm -rf *.ko *.mod.c *.o *.mod.o .*.cmd .tmp_versions test Module.symvers
	$(MAKE) -C helpers clean 
