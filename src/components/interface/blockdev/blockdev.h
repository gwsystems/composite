#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <cos_component.h>
#include <memmgr.h>

unsigned long blockdev_bread(void *buf, unsigned long offset, unsigned long length);
unsigned long COS_STUB_DECL(blockdev_bread)(void *buf, unsigned long offset, unsigned long length);

unsigned long blockdev_bwrite(const void *buf, unsigned long offset, unsigned long length);
unsigned long COS_STUB_DECL(blockdev_bwrite)(const void *buf, unsigned long offset, unsigned long length);

CCTOR int __blockdev_c_smem_init();

int __blockdev_s_smem_init(cbuf_t cid);
int COS_STUB_DECL(__blockdev_s_smem_init)(cbuf_t cid);

#endif /* BLOCKDEV_H */
