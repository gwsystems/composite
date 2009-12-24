#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

size_t __libc_getpagesize(void);
size_t __libc_getpagesize(void) {
  return PAGE_SIZE;
}

size_t getpagesize(void)       __attribute__((weak,alias("__libc_getpagesize")));

