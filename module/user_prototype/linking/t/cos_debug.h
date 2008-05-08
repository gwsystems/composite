#ifndef COS_DEBUG_H

#include <cos_component.h>

#define PRINT_FN print
#include <print.h>
#define DEBUG
#ifdef DEBUG
//extern int PRINT_FN(char *, int a, int b, int c);
#define assert(node) if (!(node)) {PRINT_FN("assert error @ %d in %d%d.\n", (__LINE__), 0/*(__FILE__)*/, 0); *((int *)0) = 0;}
//#define printd(str,args...) PRINT_FNS(str, ## args)
#define printd(str,a,b,c) PRINT_FNS(str,a,b,c)
#else 
#define assert(n)
#define printd(s,a...)
#endif 

#endif
