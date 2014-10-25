#include <sys/types.h>
#include <sys/wait.h>
#include "dietstdio.h"

int pclose(FILE *f) {
  int status;
  fclose(f);
  if (waitpid(f->popen_kludge,&status,0)>=0)
    return status;
  return -1;
}
