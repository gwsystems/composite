#include "dietfeatures.h"

#ifdef WANT_DYNAMIC
#include <sys/cdefs.h>
#include <endian.h>

typedef void(*structor)(void);

__attribute__((section(".ctors")))
__attribute_used
static structor __CTOR_END__[1]={((structor)0)};

__attribute__((section(".dtors")))
__attribute_used
static structor __DTOR_END__[1]={((structor)0)};

__attribute__((section(".eh_frame")))
__attribute_used
#if __WORDSIZE == 32
char __FRAME_END__[4] = { 0, 0, 0, 0 };
#else
char __FRAME_END__[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#endif

static void __do_global_ctors_aux(void)
{
  structor *cf=__DTOR_END__;	/* ugly trick to prevent warning */
  for(cf=((__CTOR_END__)-1); (*cf) != (structor)-1; cf--) (*cf)();
}

void _init(void) __attribute__((section(".init")));
__attribute__((section(".init"))) void _init(void)
{
  __do_global_ctors_aux();
}
#endif
