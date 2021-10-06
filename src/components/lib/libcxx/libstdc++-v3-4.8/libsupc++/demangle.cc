#include <bits/c++config.h>

#if _GLIBCXX_HOSTED

#include <cxxabi.h>

namespace __cxxabiv1
{  

char *__cxa_demangle(char const *, char *, size_t *, int *status)
{
  if (status) *status = -1;
  return 0;
}

}
#endif

