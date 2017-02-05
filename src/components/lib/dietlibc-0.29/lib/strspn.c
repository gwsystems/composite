#include <sys/types.h>
#include <string.h>

size_t strspn(const char *s, const char *accept)
{
  size_t l=0;
  int a=1,i,al=strlen(accept);

  while((a)&&(*s))
  {
    for(a=i=0;(!a)&&(i<al);i++)
      if (*s==accept[i]) a=1;
    if (a) l++;
    s++;
  }
  return l;
}
