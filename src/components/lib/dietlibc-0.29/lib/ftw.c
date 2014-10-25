#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <ftw.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include "dietdirent.h"

int ftw(const char*dir,int(*f)(const char*file,const struct stat*sb,int flag),int dpth){
  char* cd;
  size_t cdl;
  DIR* d;
  struct dirent* de;
  struct stat sb;
  int r;
  unsigned int oldlen=0;
  char* filename = NULL;
  if(chdir(dir))return-1;
  cd=alloca(PATH_MAX+1);
  if(!getcwd(cd,PATH_MAX))return-1;
  cd[PATH_MAX]='\0';
  cdl=strlen(cd);
  if(!(d=opendir(".")))return-1;
  while((de=readdir(d))){
    int flg;
    size_t nl;
    if(de->d_name[0]=='.'){if(!de->d_name[1])continue;if(de->d_name[1]=='.'&&!de->d_name[2])continue;}
    nl=strlen(de->d_name);
    if (nl+cdl+2>oldlen)
      filename=alloca(oldlen=nl+cdl+2);
    memmove(filename,cd,cdl);
    filename[cdl]='/';
    memmove(filename+cdl+1,de->d_name,nl+1);
    if(!lstat(de->d_name,&sb)){
      if(S_ISLNK(sb.st_mode))flg=FTW_SL;else if(S_ISDIR(sb.st_mode))flg=FTW_D;else flg=FTW_F;
    }else flg=FTW_NS;
    r=f(filename,&sb,flg);
    if(r){closedir(d);return r;}
    if(flg==FTW_D&&dpth){
      r=ftw(filename,f,dpth-1);
      fchdir(d->fd);
      if (r){closedir(d);return r;}
    }
  }
  return closedir(d);
}
