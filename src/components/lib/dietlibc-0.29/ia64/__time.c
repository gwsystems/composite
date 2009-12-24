#include <time.h>
#include <sys/time.h>

time_t time(time_t*t) {
  struct timeval tv;
  time_t ret;
  if (gettimeofday(&tv,0)) {
    ret=(time_t)-1;
  } else {
    ret=(time_t)tv.tv_sec;
  }
  if (t) *t=ret;
  return ret;
}
