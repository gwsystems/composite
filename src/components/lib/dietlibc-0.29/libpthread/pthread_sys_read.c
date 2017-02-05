#include <unistd.h>

#include <pthread.h>
#include "thread_internal.h"

int read(int fd, void *buf, size_t count) {
  __TEST_CANCEL();
  return __libc_read(fd,buf,count);
}
