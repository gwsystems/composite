#include <string.h>
#include <stdlib.h>

char *strdup(const char *s) {
  char *tmp=(char *)malloc(strlen(s)+1);
  if (!tmp) return 0;
  strcpy(tmp,s);
  return tmp;
}
