#define _GNU_SOURCE
#include <stdio.h>
#include "dietwarning.h"

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
  return getdelim(lineptr,n,'\n',stream);
}
