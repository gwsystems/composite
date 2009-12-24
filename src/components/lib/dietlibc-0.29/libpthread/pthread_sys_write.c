#include <unistd.h>

#include <pthread.h>
#include "thread_internal.h"

int write(int fd, const void *buf, size_t count) {
  __TEST_CANCEL();
  return __libc_write(fd,buf,count);
}
