obj-m := cos.o
cos-objs := kern_entry.o ipc.o inv.o thread.o hijack.o spd.o measurement.o page_pool.o mmap.o
KDIR=/scratch/gabep1/linux-2.6.22-current
#/scratch/gabep1/linux-2.6.13.noglobals
#/scratch/gabep1/other/linux-2.6.13
MAKE=make
LINKER_DIR=./user_prototype/linking/
NET_DIR=./net/
TRANS_DIR=/home/grad3/gabep1/transfer/

default: module

module: comps
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
	$(MAKE) -C $(NET_DIR)

cp:
	cp cos.ko $(TRANS_DIR)
	make -C $(LINKER_DIR) copy
	make -C $(NET_DIR) copy

comps:
	$(MAKE) -C $(LINKER_DIR)

loc:
	@ wc -l *.c *.S *.h include/*.h $(LINKER_DIR)/*.c $(LINKER_DIR)/t/*.S $(LINKER_DIR)/t/*.c $(LINKER_DIR)/t/*.h $(NET_DIR)/*.c $(NET_DIR)/util/*.c | tail -n 1 | awk '{print $1}'

reload: clean module cp

clean:
	rm -rf *.ko *.mod.c *.o *.mod.o .*.cmd .tmp_versions test
	$(MAKE) -C $(LINKER_DIR) clean
	$(MAKE) -C $(NET_DIR) clean
