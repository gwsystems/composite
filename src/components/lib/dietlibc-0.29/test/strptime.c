#define _XOPEN_SOURCE
#include <time.h>
#include <stdio.h>

int main() {
  char buf[1024];
  struct tm* t;
  time_t T=time(0);
  t=localtime(&T);
  
  strftime(buf,sizeof(buf),"%c",t);
  printf("%s\n",strptime(buf,"%c",t));

  return 0;
}
