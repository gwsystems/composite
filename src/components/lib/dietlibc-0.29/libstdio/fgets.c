#include "dietstdio.h"

char *fgets_unlocked(char *s, int size, FILE *stream) {
  char *orig=s;
  int l;
  for (l=size; l>1;) {
    register int c=fgetc_unlocked(stream);
    if (c==EOF) break;
    *s=c;
    ++s;
    --l;
    if (c=='\n') break;
  }
  if (l==size || ferror_unlocked(stream))
    return 0;
  *s=0;
  return orig;
}

char*fgets(char*s,int size,FILE*stream) __attribute__((weak,alias("fgets_unlocked")));
