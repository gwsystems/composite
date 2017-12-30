typedef int cos_eal_thd_t;
cos_eal_thd_t cos_eal_thd_curr(void);
int cos_eal_thd_create(cos_eal_thd_t *, void *(*)(void *), void *);
