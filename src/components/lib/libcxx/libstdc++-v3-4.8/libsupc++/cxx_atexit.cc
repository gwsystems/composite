#include "cxx_atexit.h"

#define NUM_ATEXIT	64

struct __exit_handler
{
  void (*f)(void *);
  void *arg;
  void *dso_handle;
};

static __exit_handler __atexitlist[NUM_ATEXIT];
static volatile unsigned atexit_counter;
void *__dso_handle __attribute__((weak));

int __cxa_atexit(void (*f)(void*), void *arg, void *dso_handle)
{
  unsigned c = atexit_counter++;
  if (c >= NUM_ATEXIT)
    return -1;

  __atexitlist[c].f = f;
  __atexitlist[c].arg = arg;
  __atexitlist[c].dso_handle = dso_handle;

  return 0;
}



