#include <stdlib.h>
#include <string.h>
#include <errno.h>

int putenv(const char *string) {
  size_t len;
  int envc;
  int remove=0;
  char *tmp;
  const char **ep;
  char **newenv;
  static char **origenv;
  if (!origenv) origenv=environ;
  if (!(tmp=strchr(string,'='))) {
    len=strlen(string);
    remove=1;
  } else
    len=tmp-string+1;
  for (envc=0, ep=(const char**)environ; *ep; ++ep) {
    if (*string == **ep && !memcmp(string,*ep,len)) {
      if (remove) {
	for (; ep[1]; ++ep) ep[0]=ep[1];
	ep[0]=0;
	return 0;
      }
      *ep=string;
      return 0;
    }
    ++envc;
  }
  if (tmp) {
    newenv = (char**) realloc(environ==origenv?0:origenv,
			      (envc+2)*sizeof(char*));
    if (!newenv) return -1;
    newenv[0]=(char*)string;
    memcpy(newenv+1,environ,(envc+1)*sizeof(char*));
    environ=newenv;
  }
  return 0;
}
