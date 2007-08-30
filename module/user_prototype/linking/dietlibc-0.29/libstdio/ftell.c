#include <dietstdio.h>
#include <unistd.h>
#include <errno.h>

long ftell_unlocked(FILE *stream) {
  off_t l;
  if (fflush_unlocked(stream)) return -1;
  l=lseek(stream->fd,0,SEEK_CUR);
  if (l==-1) return -1;
  return l-stream->ungotten;
}

long ftell(FILE *stream) __attribute__((weak,alias("ftell_unlocked")));
