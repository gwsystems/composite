#define COS_DEBUG

#ifdef __KERNEL__
#ifdef COS_DEBUG
#define assert(node) WARN_ON(!(node)) //if (!(node)) {printk("cos error: %d in %s.\n", (__LINE__), (__FILE__)); *((int *)0) = 0;}
#define printd(str,args...) printk(str, ## args)
/*#else
  #define printd(str,args...) printf(str, ## args)*/
#else
#define assert(a)
#define printd(str,args...) 
#endif
#endif

