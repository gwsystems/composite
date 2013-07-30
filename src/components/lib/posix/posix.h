#ifndef POSIX_H
#define POSIX_H

#include <stdio.h>
#include "../../interface/torrent/torrent.h"
#include "../../interface/evt/evt.h"
//#include <sys/stat.h>
#include "../dietlibc-0.29/include/cos_syscalls.h"

void posix_init(void);

// torrent related
int cos_open(const char *pathname, int flags, int mode);        // FLAGS not implemented, however bbfedtect only use O_RDONLY 
int cos_close(int fd);                                          // no error ret
ssize_t cos_read(int fd, void *buf, size_t count);              // done
ssize_t cos_write(int fd, const void *buf, size_t count);       // done
off_t cos_lseek(int fd, off_t offset, int whence);              // SEEK_END flag is not implemented, logic should be reworked
//int cos_fstat(int fd, struct stat *buf);                        // (not) done should alwas be success(0)

// cos_alloc related
void *cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int cos_munmap(void *start, size_t length);
void *cos_mremap(void *old_address, size_t old_size, size_t new_size, int flags);

// default
int default_syscall(void);

#endif
