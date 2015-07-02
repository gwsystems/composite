include Makefile.src

#EXTRA_CFLAGS=-DCOS_LINUX -Wno-unused
EXTRA_CFLAGS=-Wno-unused

obj-m := cos.o
cos-objs := kern_entry.o hw_ints.o hijack.o pgtbl.o ../../../kernel/inv.o ../../../kernel/capinv.o ../../../kernel/liveness_tbl.o ../../../kernel/pgtbl.o ../../../kernel/captbl.o ../../../kernel/retype_tbl.o ../../../kernel/thread.o ../../../kernel/spd.o ../../../kernel/per_cpu.o ../../../kernel/measurement.o ../../../kernel/page_pool.o ../../../kernel/mmap.o ../../../kernel/ring_buff.o ../../../kernel/fpu.o

default: module

module: 
	$(MAKE) -W -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -C $(LDIR) SUBDIRS=$(shell pwd) modules

cp:
	cp cos.ko $(TRANS_DIR)

#loc:
#	@ wc -l *.c *.S *.h include/*.h $(LINKER_DIR)/*.c $(LINKER_DIR)/t/*.S $(LINKER_DIR)/t/*.c $(LINKER_DIR)/t/*.h $(NET_DIR)/*.c $(NET_DIR)/util/*.c | tail -n 1 | awk '{print $1}'

clean:
	rm -rf *.ko *.mod.c *.o *.mod.o .*.cmd .tmp_versions test Module.symvers
	rm -rf $(KDIR)/*.o $(KDIR)/.*.o.cmd

fresh: clean default
