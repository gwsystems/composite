obj-m := cos.o
cos-objs := kern_entry.o ipc.o inv.o thread.o hijack.o spd.o measurement.o
KDIR=/scratch/gabep1/linux-2.6.22-current
#/scratch/gabep1/linux-2.6.13.noglobals
#/scratch/gabep1/other/linux-2.6.13
MAKE=make

default: module 

module:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -rf *.ko *.mod.c *.o *.mod.o .*.cmd .tmp_versions test
