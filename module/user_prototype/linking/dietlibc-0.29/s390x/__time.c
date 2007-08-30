#include <time.h>
#include <sys/time.h>

time_t time(time_t *t) {
  struct timeval tv;
  if (gettimeofday(&tv, 0) == -1)
    tv.tv_sec=-1;
  if (t) *t=tv.tv_sec;
  return tv.tv_sec;
}
