#define _GNU_SOURCE
/* *puke* */
#include <string.h>
#include <stdlib.h>

char *strndup(const char *s,size_t n) {
  char *tmp=(char *)malloc(n+1);
  if (!tmp) return 0;
  strncpy(tmp,s,n);
  tmp[n]=0;
  return tmp;
}
