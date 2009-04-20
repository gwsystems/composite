#define COS_DEBUG

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

