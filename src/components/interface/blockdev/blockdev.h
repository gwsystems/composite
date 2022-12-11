#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <cos_component.h>
#include <memmgr.h>

/**
 * Blockdev interface for now only has two public function.
 */

/**
 * Read from the blockdev
 * - @buf: pointer to the buffer
 * - @offset: the offset
 * - @length: the length
 * - @return: status code of this function
 */
unsigned long blockdev_bread(void *buf, unsigned long offset, unsigned long length);
unsigned long COS_STUB_DECL(blockdev_bread)(void *buf, unsigned long offset, unsigned long length);

/**
 * Write to the blockdev
 * - @buf: pointer to the buffer
 * - @offset: the offset
 * - @length: the length
 * - @return: status code of this function
 */
unsigned long blockdev_bwrite(const void *buf, unsigned long offset, unsigned long length);
unsigned long COS_STUB_DECL(blockdev_bwrite)(const void *buf, unsigned long offset, unsigned long length);

/**
 * Internal function to init the share memory on the client side
 */
CCTOR int __blockdev_c_smem_init();

/**
 * Internal function to init the share memory on the server side
 */
int __blockdev_s_smem_init(cbuf_t cid);
int COS_STUB_DECL(__blockdev_s_smem_init)(cbuf_t cid);

#endif /* BLOCKDEV_H */
