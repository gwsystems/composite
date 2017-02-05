#include "dietdirent.h"
#include <unistd.h>
#include <dirent.h>

off_t telldir(DIR *d) {
  return lseek(d->fd,0,SEEK_CUR)-d->num+d->cur;
}
