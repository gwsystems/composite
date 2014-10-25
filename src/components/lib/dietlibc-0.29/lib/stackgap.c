#include <unistd.h>
#include <fcntl.h>
#include <alloca.h>
#include "dietfeatures.h"

extern int main(int argc,char* argv[],char* envp[]);

long __guard;

int stackgap(int argc,char* argv[],char* envp[]);
int stackgap(int argc,char* argv[],char* envp[]) {
  int fd=open("/dev/urandom",O_RDONLY);
  unsigned short s;
  volatile char* gap;
  read(fd,&s,2);
  read(fd,&__guard,sizeof(__guard));
  close(fd);
  gap=alloca(s);
  return main(argc,argv,envp);
}

