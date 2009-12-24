#include "dietfeatures.h"

#ifdef WANT_DYNAMIC
#include <sys/cdefs.h>
#include <stdlib.h>

struct object {
  void *detail[7]; /* see gcc sources unwind-dw2-fde.h */
};

extern void __register_frame_info (const void *, struct object *) __attribute__((weak));
extern void __deregister_frame_info (const void *) __attribute__((weak));


typedef void(*structor)(void);

__attribute__((section(".ctors")))
__attribute_used
static structor __CTOR_LIST__[1]={((structor)-1)};

__attribute__((section(".dtors")))
__attribute_used
static structor __DTOR_LIST__[1]={((structor)-1)};

__attribute__((section(".eh_frame")))
__attribute_used
char __EH_FRAME_BEGIN__[] = { };


static void __do_global_dtors_aux(void)
{
  structor *df=__CTOR_LIST__;	/* ugly trick to prevent warning */
  for (df=((__DTOR_LIST__)+1);(*df) != (structor)0; df++) (*df)();
}

void _fini(void) __attribute__((section(".fini")));
__attribute__((section(".fini"))) void _fini(void)
{
  __do_global_dtors_aux();
  if (__deregister_frame_info)
    __deregister_frame_info(__EH_FRAME_BEGIN__);
}

#ifdef WANT_STACKGAP
int stackgap(int argc,char* argv[],char* envp[]);
#endif

#ifndef __DYN_LIB_SHARED
/* pre main, post _start */
extern __attribute__((section(".init"))) void _init(void);

int _dyn_start(int argc, char **argv, char **envp, structor dl_init);
int _dyn_start(int argc, char **argv, char **envp, structor dl_init)
{
  int main(int argc, char **argv, char **envp);

  void _dl_aux_init_from_envp(char **envp);
  _dl_aux_init_from_envp(envp);

  if (dl_init) atexit(dl_init);
  _init();
  atexit(_fini);

  if (__register_frame_info) {
    static struct object ob;
    __register_frame_info(__EH_FRAME_BEGIN__, &ob);
  }

#ifdef WANT_STACKGAP
  return stackgap(argc, argv, envp);
#else
  return main(argc, argv, envp);
#endif
}
#endif
#endif
