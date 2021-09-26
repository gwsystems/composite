#include <cos_component.h>
#include <llprint.h>
#include <memmgr.h>

#define BLOCKDEV_RAMDISK_BSIZE 4096 // 4K block size
#define BLOCKDEV_RAMDISK_SIZE 16 * 1024 * 1024 // 16 MiB ramdisk size

typedef struct block {
	char c[BLOCKDEV_RAMDISK_BSIZE];
} blk;

blk *ram_backend;

unsigned int
blockdev_bread(void *buf, unsigned long offset, unsigned long length)
{
	memcpy(buf, &ram_backend[offset], length * BLOCKDEV_RAMDISK_BSIZE);
	return 0;
}

unsigned int
blockdev_bwrite(const void *buf, unsigned long offset, unsigned long length)
{
	memcpy(&ram_backend[offset], buf, length * BLOCKDEV_RAMDISK_BSIZE);
	return 0;
}

void
cos_init(void)
{
	ram_backend = (blk *)memmgr_heap_page_allocn(BLOCKDEV_RAMDISK_SIZE / 4096);
	return;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	return;
}

void
parallel_main(coreid_t cid)
{
	return;
}