#ifndef _SYS_VFS_H
#define _SYS_VFS_H

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

typedef struct {
  int32_t __val[2];
} __kernel_fsid_t;

struct statfs {
  long f_type;
  long f_bsize;
#ifdef __mips__
  long f_frsize;
#endif
  long f_blocks;
  long f_bfree;
#ifndef __mips__
  long f_bavail;
#endif
  long f_files;
  long f_ffree;
#ifdef __mips__
  long f_bavail;
#endif
  __kernel_fsid_t f_fsid;
  long f_namelen;
  long f_spare[6];
};

int statfs(const char *path, struct statfs *buf) __THROW;
int fstatfs(int fd, struct statfs *buf) __THROW;

__END_DECLS

#endif
