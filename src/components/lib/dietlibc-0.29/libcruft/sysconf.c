#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/resource.h>

extern int __sc_nr_cpus();

long sysconf(int name)
{
  switch(name)
  {
  case _SC_OPEN_MAX:
    {
      struct rlimit limit;
      getrlimit(RLIMIT_NOFILE, &limit);
      return limit.rlim_cur;
    }
  case _SC_CLK_TCK:
#ifdef __alpha__
    return 1024;
#else
    return 100;
#endif

  case _SC_PAGESIZE:
#if ( defined(__alpha__) || defined(__sparc__) )
    return 8192;
#else
    return 4096;
#endif

  case _SC_ARG_MAX:
    return ARG_MAX;

  case _SC_NGROUPS_MAX:
    return NGROUPS_MAX;

  case _SC_NPROCESSORS_ONLN:
    return __sc_nr_cpus();

  }
  return -1;
}
