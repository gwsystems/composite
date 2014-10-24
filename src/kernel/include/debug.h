#include "shared/cos_config.h"
#ifdef COS_ASSERTIONS_ACTIVE
#define COS_DEBUG
#endif

#ifndef unlikely
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif

#ifndef likely
#define likely(x)       __builtin_expect(!!(x), 1)
#endif

#ifdef __KERNEL__
#ifdef COS_DEBUG
#define assert(node) \
	do {								\
		if(likely((node))) break;				\
		WARN_ON(unlikely(!(node)));				\
		BUG();							\
	} while(0);
#define printd(str,args...) printk(str, ## args)
/*#else
  #define printd(str,args...) printf(str, ## args)*/
#else
#define assert(a)
#define printd(str,args...) 
#endif
#endif

#ifndef assert
#define assert(a)
#endif
