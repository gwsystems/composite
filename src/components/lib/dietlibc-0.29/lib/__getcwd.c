#include <unistd.h>
#include <stdlib.h>

extern int __syscall_getcwd(char* buf, size_t size);

char *getcwd(char *buf, size_t size) {
  int tmp;
  if ((tmp=__syscall_getcwd(buf,size))<0) return 0;
  buf[tmp]=0;
  return buf;
}
