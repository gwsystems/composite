#include <ext4_config.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include <blockdev.h>

/**
 * Use blockdev interface to provide a block device for lwext4
 */

#define EXT4_BLOCKDEV_BSIZE 4096            // 4KiB
#define EXT4_BLOCKDEV_SIZE 16 * 1024 * 1024 // 16KiB

static int __blockdev_open(struct ext4_blockdev *bdev);
static int __blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);
static int __blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt);
static int __blockdev_close(struct ext4_blockdev *bdev);

EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blockdev, EXT4_BLOCKDEV_BSIZE, EXT4_BLOCKDEV_SIZE / EXT4_BLOCKDEV_BSIZE,
                              __blockdev_open, __blockdev_bread, __blockdev_bwrite, __blockdev_close, 0, 0);

static int
__blockdev_open(struct ext4_blockdev *bdev)
{
	return EOK;
}

static int
__blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
	blockdev_bread(buf, blk_id, blk_cnt);
	return EOK;
}

static int
__blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
	blockdev_bwrite(buf, blk_id, blk_cnt);
	return EOK;
}

static int
__blockdev_close(struct ext4_blockdev *bdev)
{
	return EOK;
}

struct ext4_blockdev *
ext4_blockdev_get(void)
{
	return &ext4_blockdev;
}