#include "dietdirent.h"
#include <unistd.h>
#include <dirent.h>

void seekdir(DIR *d,off_t offset) {
  if (lseek(d->fd,offset,SEEK_SET) != (off_t)-1)
    d->num=d->cur=0;
}
