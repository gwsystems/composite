include Makefile.src

obj-m := trans.o
cos-objs := translator.o

default: module

module: 
	$(MAKE) -C $(LDIR) SUBDIRS=$(shell pwd) modules

cp:
	cp trans.ko $(TRANS_DIR)

clean:
	rm -rf *.ko *.mod.c *.o *.mod.o .*.cmd .tmp_versions test Module.symvers
	rm -rf $(KDIR)/*.o $(KDIR)/.*.o.cmd

fresh: clean default
