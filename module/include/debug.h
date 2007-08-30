#define DEBUG
#ifdef DEBUG
#ifdef __KERNEL__
#define printd(str,args...) printk(str, ## args)
#else
#define printd(str,args...) printf(str, ## args)
#endif
#else
#define printd(str,args...) 
#endif
